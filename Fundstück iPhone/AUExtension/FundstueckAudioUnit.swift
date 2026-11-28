import AVFoundation
import AudioToolbox

/// AUv3 instrument (aumu): 8 pads on MIDI notes, built-in host-synced step sequencer.
public final class FundstueckAudioUnit: AUAudioUnit {

    let drum: DrumEngine
    var onKitChanged: (() -> Void)?
    private(set) var kitName = "Kit"

    private var outputBus: AUAudioUnitBus!
    private var outputBusArray: AUAudioUnitBusArray!
    private let render = RenderContext()

    /// Everything the render block touches lives here – no access to `self` on the audio thread.
    final class RenderContext {
        var musicalContext: AUHostMusicalContextBlock?
        var transportState: AUHostTransportStateBlock?
        var capacity: AUAudioFrameCount = 0
        var left: UnsafeMutablePointer<Float>
        var right: UnsafeMutablePointer<Float>

        init() {
            left = .allocate(capacity: 1)
            right = .allocate(capacity: 1)
            ensure(4096)
        }

        func ensure(_ frames: AUAudioFrameCount) {
            guard frames > capacity else { return }
            left.deallocate(); right.deallocate()
            left = .allocate(capacity: Int(frames))
            right = .allocate(capacity: Int(frames))
            left.initialize(repeating: 0, count: Int(frames))
            right.initialize(repeating: 0, count: Int(frames))
            capacity = frames
        }

        deinit { left.deallocate(); right.deallocate() }
    }

    static let padNames = ["Kick", "Snare", "Clap", "Hat", "Open Hat", "Tom Lo", "Tom Hi", "Perc"]

    public override init(componentDescription: AudioComponentDescription,
                         options: AudioComponentInstantiationOptions = []) throws {
        let sampleRate = 44100.0
        drum = DrumEngine(sampleRate: sampleRate)
        try super.init(componentDescription: componentDescription, options: options)

        guard let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2) else {
            throw NSError(domain: NSOSStatusErrorDomain, code: Int(kAudioUnitErr_FormatNotSupported))
        }
        outputBus = try AUAudioUnitBus(format: format)
        outputBus.maximumChannelCount = 2
        outputBusArray = AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [outputBus])

        let tree = FundstueckAudioUnit.makeParameterTree()
        let ref = drum.ref
        tree.implementorValueObserver = { param, value in
            fs_engine_set_param(ref, Int32(param.address), value)
        }
        tree.implementorValueProvider = { param in
            fs_engine_get_param(ref, Int32(param.address))
        }
        tree.implementorStringFromValueCallback = { param, valuePtr in
            let v = valuePtr?.pointee ?? param.value
            switch param.unit {
            case .boolean: return v > 0.5 ? "On" : "Off"
            case .relativeSemiTones: return String(format: "%+.1f st", v)
            case .BPM: return String(format: "%.0f BPM", v)
            case .percent: return String(format: "%.0f %%", v)
            default: return String(format: "%.2f", v)
            }
        }
        parameterTree = tree

        // Start with the newest kit from the app (shared App Group) or a demo kit.
        if let entry = KitStore.list().first, let data = KitStore.load(entry), drum.load(data: data) {
            kitName = entry.name
        } else {
            drum.loadDemoKit()
            kitName = "Demo-Kit"
        }
        drum.setParam(FS_PARAM_PLAY, 1)       // pattern runs with the host transport
        drum.setParam(FS_PARAM_FOLLOW_HOST, 1)
        maximumFramesToRender = 4096
    }

    private static func makeParameterTree() -> AUParameterTree {
        let flags: AudioUnitParameterOptions = [.flag_IsReadable, .flag_IsWritable, .flag_CanRamp]
        func param(_ id: String, _ name: String, _ address: Int32, _ min: Float, _ max: Float,
                   _ unit: AudioUnitParameterUnit) -> AUParameter {
            AUParameterTree.createParameter(withIdentifier: id, name: name,
                                            address: AUParameterAddress(address),
                                            min: min, max: max, unit: unit, unitName: nil,
                                            flags: flags, valueStrings: nil, dependentParameters: nil)
        }
        let global = AUParameterTree.createGroup(withIdentifier: "global", name: "Global", children: [
            param("master", "Master", FS_PARAM_MASTER, 0, 1.5, .linearGain),
            param("drive", "Drive", FS_PARAM_DRIVE, 0, 1, .generic),
            param("swing", "Swing", FS_PARAM_SWING, 0, 0.5, .generic),
            param("tempo", "Tempo (intern)", FS_PARAM_TEMPO, 60, 200, .BPM),
            param("pattern", "Pattern an", FS_PARAM_PLAY, 0, 1, .boolean),
            param("followHost", "Host folgen", FS_PARAM_FOLLOW_HOST, 0, 1, .boolean),
        ])
        var groups: [AUParameterNode] = [global]
        for pad in 0..<DrumEngine.padCount {
            let n = padNames[pad]
            groups.append(AUParameterTree.createGroup(withIdentifier: "pad\(pad)", name: n, children: [
                param("pad\(pad)gain", "\(n) Level", DrumEngine.padParam(pad, FS_PAD_GAIN), 0, 1.5, .linearGain),
                param("pad\(pad)tune", "\(n) Tune", DrumEngine.padParam(pad, FS_PAD_TUNE), -24, 24, .relativeSemiTones),
                param("pad\(pad)decay", "\(n) Decay", DrumEngine.padParam(pad, FS_PAD_DECAY), 0.05, 1, .generic),
                param("pad\(pad)pan", "\(n) Pan", DrumEngine.padParam(pad, FS_PAD_PAN), -1, 1, .pan),
            ]))
        }
        return AUParameterTree.createTree(withChildren: groups)
    }

    // MARK: Busses & resources

    public override var outputBusses: AUAudioUnitBusArray { outputBusArray }

    public override var channelCapabilities: [NSNumber]? { [0, 2, 0, 1] }

    public override var canProcessInPlace: Bool { false }

    public override func allocateRenderResources() throws {
        try super.allocateRenderResources()
        drum.setSampleRate(outputBus.format.sampleRate)
        render.ensure(maximumFramesToRender)
        render.musicalContext = musicalContextBlock
        render.transportState = transportStateBlock
    }

    public override func deallocateRenderResources() {
        render.musicalContext = nil
        render.transportState = nil
        super.deallocateRenderResources()
    }

    // MARK: Render

    public override var internalRenderBlock: AUInternalRenderBlock {
        let ref = drum.ref
        let ctx = render
        return { _, timestamp, frameCount, _, outputData, realtimeEventListHead, _ in
            guard frameCount <= ctx.capacity else { return kAudioUnitErr_TooManyFramesToProcess }

            // MIDI + parameter events, sample accurate
            let bufferStart = AUEventSampleTime(timestamp.pointee.mSampleTime)
            var event = realtimeEventListHead
            while let ev = event {
                let head = ev.pointee.head
                var offset = head.eventSampleTime - bufferStart
                if offset >= AUEventSampleTime(frameCount) { offset = AUEventSampleTime(frameCount) - 1 }
                if offset < 0 { offset = 0 }
                switch head.eventType {
                case .MIDI:
                    let m = ev.pointee.MIDI
                    fs_engine_queue_midi(ref, m.data.0, m.data.1, m.data.2, UInt32(offset))
                case .parameter, .parameterRamp:
                    let p = ev.pointee.parameter
                    fs_engine_set_param(ref, Int32(p.parameterAddress), p.value)
                default:
                    break
                }
                event = UnsafePointer(head.next)
            }

            // Host tempo & transport
            var transport = fs_transport()
            if let musical = ctx.musicalContext {
                var tempo = 0.0, beat = 0.0
                if musical(&tempo, nil, nil, &beat, nil, nil) {
                    transport.hostValid = 1
                    transport.tempo = tempo
                    transport.beatPosition = beat
                }
            }
            if transport.hostValid != 0 {
                if let state = ctx.transportState {
                    var flags = AUHostTransportStateFlags()
                    if state(&flags, nil, nil, nil) {
                        transport.hostPlaying = flags.contains(.moving) ? 1 : 0
                    } else {
                        transport.hostPlaying = 1
                    }
                } else {
                    transport.hostPlaying = 1
                }
            }

            fs_engine_render(ref, ctx.left, ctx.right, frameCount, &transport)

            let out = UnsafeMutableAudioBufferListPointer(outputData)
            let bytes = Int(frameCount) * MemoryLayout<Float>.size
            if out.count == 1 {
                for i in 0..<Int(frameCount) { ctx.left[i] = 0.5 * (ctx.left[i] + ctx.right[i]) }
            }
            for i in 0..<min(out.count, 2) {
                let src = i == 0 ? ctx.left : ctx.right
                if let dst = out[i].mData {
                    dst.copyMemory(from: src, byteCount: bytes)
                } else {
                    out[i].mData = UnsafeMutableRawPointer(src)
                }
                out[i].mDataByteSize = UInt32(bytes)
            }
            return noErr
        }
    }

    // MARK: State (kit is stored inside the host project)

    private static let stateKey = "fundstueckKit"
    private static let nameKey = "fundstueckKitName"

    public override var fullState: [String: Any]? {
        get {
            var state = super.fullState ?? [:]
            state[FundstueckAudioUnit.stateKey] = drum.exportData()
            state[FundstueckAudioUnit.nameKey] = kitName
            return state
        }
        set {
            super.fullState = newValue
            if let data = newValue?[FundstueckAudioUnit.stateKey] as? Data, drum.load(data: data) {
                kitName = newValue?[FundstueckAudioUnit.nameKey] as? String ?? "Kit"
                notifyKitChanged()
            }
        }
    }

    func loadKit(_ entry: KitStore.Entry, keepGroove: Bool) -> Bool {
        guard let data = KitStore.load(entry), drum.load(data: data, keepState: keepGroove) else { return false }
        kitName = entry.name
        notifyKitChanged()
        return true
    }

    private func notifyKitChanged() {
        DispatchQueue.main.async { [weak self] in self?.onKitChanged?() }
    }
}
