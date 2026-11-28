import AVFoundation
import SwiftUI

/// Owns the app's audio engine: mic recording + playback of the drum engine.
final class AudioController: ObservableObject {
    let drum: DrumEngine
    let kit: KitModel

    @Published var isRecording = false
    @Published var isBuilding = false
    @Published var inputLevel: Float = 0
    @Published var overview: [Float] = []          // recording waveform for display
    @Published var sliceMarkers: [Double] = []     // 0…1 positions of detected hits
    @Published var recordingSeconds: Double = 0
    @Published var message: String?
    @Published var micDenied = false

    // analysis settings
    @Published var sensitivity: Float = 0.5
    @Published var character: Float = 0.65
    @Published var keepGroove = false

    private(set) var recording: [Float] = []
    private(set) var recordingSampleRate: Double = 48000

    private let engine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode?
    private var captureBuffer: [Float] = []
    private let captureLock = NSLock()
    private let maxSeconds: Double = 30
    private var seed: UInt32 = 0

    init() {
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker, .allowBluetoothA2DP, .mixWithOthers])
        try? session.setPreferredIOBufferDuration(0.005)
        try? session.setActive(true)
        let sr = session.sampleRate > 0 ? session.sampleRate : 48000
        let drumEngine = DrumEngine(sampleRate: sr)
        var initialName = "Demo-Kit"
        if let entry = KitStore.list().first, let data = KitStore.load(entry), drumEngine.load(data: data) {
            initialName = entry.name
        } else {
            drumEngine.loadDemoKit()
        }
        drum = drumEngine
        kit = KitModel(engine: drumEngine)
        kit.kitName = initialName
        setupPlayback(sampleRate: sr)
        session.requestRecordPermission { [weak self] granted in
            DispatchQueue.main.async {
                self?.micDenied = !granted
                self?.startEngine()
            }
        }
        NotificationCenter.default.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  AVAudioSession.InterruptionType(rawValue: raw) == .ended else { return }
            try? AVAudioSession.sharedInstance().setActive(true)
            self?.startEngine()
        }
        NotificationCenter.default.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: .main) { [weak self] _ in
            self?.startEngine()
        }
    }

    private func setupPlayback(sampleRate: Double) {
        guard let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2) else { return }
        let ref = drum.ref
        let node = AVAudioSourceNode(format: format) { _, _, frameCount, abl -> OSStatus in
            let list = UnsafeMutableAudioBufferListPointer(abl)
            guard list.count >= 2,
                  let l = list[0].mData?.assumingMemoryBound(to: Float.self),
                  let r = list[1].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            var transport = fs_transport()
            fs_engine_render(ref, l, r, frameCount, &transport)
            return noErr
        }
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: format)
        sourceNode = node
    }

    private func startEngine() {
        guard !engine.isRunning else { return }
        if !micDenied { _ = engine.inputNode } // enables input before start
        engine.prepare()
        do { try engine.start() } catch { message = "Audio konnte nicht starten: \(error.localizedDescription)" }
    }

    // MARK: Recording

    func toggleRecording() { isRecording ? stopRecording() : startRecording() }

    func startRecording() {
        guard !micDenied else {
            message = "Mikrofonzugriff ist aus. Bitte in Einstellungen › Fundstück erlauben."
            return
        }
        startEngine()
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        guard format.sampleRate > 0, format.channelCount > 0 else {
            message = "Kein Mikrofon verfügbar."
            return
        }
        recordingSampleRate = format.sampleRate
        captureLock.lock(); captureBuffer.removeAll(keepingCapacity: true); captureLock.unlock()
        overview = []; sliceMarkers = []; recordingSeconds = 0
        let maxFrames = Int(maxSeconds * format.sampleRate)
        input.removeTap(onBus: 0)
        input.installTap(onBus: 0, bufferSize: 1024, format: format) { [weak self] buffer, _ in
            guard let self, let ch = buffer.floatChannelData else { return }
            let n = Int(buffer.frameLength)
            let p = UnsafeBufferPointer(start: ch[0], count: n)
            var peak: Float = 0
            for v in p { peak = max(peak, abs(v)) }
            self.captureLock.lock()
            self.captureBuffer.append(contentsOf: p)
            let total = self.captureBuffer.count
            self.captureLock.unlock()
            DispatchQueue.main.async {
                self.inputLevel = max(peak, self.inputLevel * 0.8)
                self.recordingSeconds = Double(total) / format.sampleRate
                self.overview.append(peak)
                if total >= maxFrames, self.isRecording { self.stopRecording() }
            }
        }
        isRecording = true
    }

    func stopRecording() {
        guard isRecording else { return }
        engine.inputNode.removeTap(onBus: 0)
        isRecording = false
        inputLevel = 0
        captureLock.lock(); recording = captureBuffer; captureBuffer = []; captureLock.unlock()
        updateOverview()
        buildKit(newSeed: false)
    }

    // MARK: Import

    func importAudio(from url: URL) {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        do {
            let file = try AVAudioFile(forReading: url)
            let format = file.processingFormat
            let frames = AVAudioFrameCount(min(file.length, AVAudioFramePosition(format.sampleRate * 60)))
            guard frames > 0, let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: frames) else { throw NSError(domain: "Fundstueck", code: 1) }
            try file.read(into: buffer, frameCount: frames)
            guard let ch = buffer.floatChannelData else { throw NSError(domain: "Fundstueck", code: 2) }
            let n = Int(buffer.frameLength), channels = Int(format.channelCount)
            var mono = [Float](repeating: 0, count: n)
            for c in 0..<channels {
                let p = ch[c]
                for i in 0..<n { mono[i] += p[i] / Float(channels) }
            }
            recording = mono
            recordingSampleRate = format.sampleRate
            recordingSeconds = Double(n) / format.sampleRate
            kit.kitName = KitStore.sanitize(url.deletingPathExtension().lastPathComponent)
            updateOverview()
            buildKit(newSeed: false)
        } catch {
            message = "Datei konnte nicht gelesen werden."
        }
    }

    private func updateOverview(bins: Int = 160) {
        guard !recording.isEmpty else { overview = []; return }
        let per = max(1, recording.count / bins)
        overview = stride(from: 0, to: recording.count, by: per).map { start in
            let end = min(start + per, recording.count)
            var m: Float = 0
            for i in start..<end { m = max(m, abs(recording[i])) }
            return m
        }
        let peak = overview.max() ?? 1
        if peak > 0 { overview = overview.map { $0 / peak } }
    }

    // MARK: Kit building

    var hasRecording: Bool { recording.count > 4096 }

    /// Analyse the recording and build a new kit (on a background queue).
    func buildKit(newSeed: Bool) {
        guard hasRecording, !isBuilding else { return }
        if newSeed { seed = UInt32.random(in: 1...UInt32.max) }
        isBuilding = true
        let samples = recording, sr = recordingSampleRate
        let sens = sensitivity, chr = character, style = Int32(kit.style), s = seed
        let keep = keepGroove
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            var slices = [fs_slice](repeating: fs_slice(), count: 256)
            let found = samples.withUnsafeBufferPointer { buf in
                slices.withUnsafeMutableBufferPointer { out in
                    fs_detect_slices(buf.baseAddress, UInt32(buf.count), sr, sens, out.baseAddress, 256)
                }
            }
            let markers = (0..<Int(max(0, found))).map { Double(slices[$0].start) / Double(samples.count) }
            let newKit = samples.withUnsafeBufferPointer { buf in
                fs_build_kit(buf.baseAddress, UInt32(buf.count), sr, sens, chr, style, s)
            }
            DispatchQueue.main.async {
                guard let self else { if let k = newKit { fs_kit_destroy(k) }; return }
                self.isBuilding = false
                self.sliceMarkers = markers
                guard let k = newKit else {
                    self.message = "Keine Schläge gefunden – lauter aufnehmen oder Empfindlichkeit erhöhen."
                    return
                }
                self.drum.load(kit: k, keepState: keep)
                self.kit.refreshFromEngine()
                self.message = "\(found) Sounds gefunden → Kit gebaut."
            }
        }
    }

    // MARK: Kits

    func saveKit(name: String) {
        let clean = KitStore.sanitize(name)
        do {
            try KitStore.save(drum.exportData(), name: clean)
            kit.kitName = clean
            message = "„\(clean)“ gespeichert – auch im AUv3-Plugin verfügbar."
        } catch {
            message = "Speichern fehlgeschlagen."
        }
    }

    func load(_ entry: KitStore.Entry) {
        guard let data = KitStore.load(entry), kit.load(data: data, name: entry.name) else {
            message = "Kit konnte nicht geladen werden."
            return
        }
    }

    func exportWAVs() -> [URL] { drum.exportWAVs(kitName: kit.kitName) }
}
