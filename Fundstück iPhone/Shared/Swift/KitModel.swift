import SwiftUI

/// UI state for a loaded kit. Shared by the app and the AUv3 view.
final class KitModel: ObservableObject {
    let engine: DrumEngine

    @Published var padNames: [String] = Array(repeating: "", count: DrumEngine.padCount)
    @Published var waveforms: [[Float]] = Array(repeating: [], count: DrumEngine.padCount)
    @Published var steps: [[UInt8]] = Array(repeating: Array(repeating: 0, count: DrumEngine.stepCount), count: DrumEngine.padCount)
    @Published var selectedPad = 0
    @Published var currentStep = -1
    @Published var flash: [Double] = Array(repeating: 0, count: DrumEngine.padCount)
    @Published var style = 0
    @Published var density: Float = 0.6
    @Published var kitName = "Kit"

    /// In the plugin, parameter changes go through the AUParameterTree so the host sees them.
    var paramSink: ((Int32, Float) -> Void)?

    private var timer: Timer?
    private var lastTriggers: [UInt32]

    init(engine: DrumEngine) {
        self.engine = engine
        lastTriggers = (0..<DrumEngine.padCount).map { engine.triggerCount(pad: $0) }
        refreshFromEngine()
        let t = Timer(timeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in self?.tick() }
        RunLoop.main.add(t, forMode: .common)
        timer = t
    }

    deinit { timer?.invalidate() }

    func refreshFromEngine() {
        padNames = (0..<DrumEngine.padCount).map { engine.padName($0) }
        waveforms = (0..<DrumEngine.padCount).map { engine.waveform(pad: $0) }
        refreshSteps()
    }

    func refreshSteps() {
        steps = (0..<DrumEngine.padCount).map { p in (0..<DrumEngine.stepCount).map { engine.step(pad: p, step: $0) } }
    }

    private func tick() {
        let cur = engine.currentStep
        if cur != currentStep { currentStep = cur }
        var newFlash = flash
        var changed = false
        for p in 0..<DrumEngine.padCount {
            let count = engine.triggerCount(pad: p)
            if count != lastTriggers[p] {
                lastTriggers[p] = count
                newFlash[p] = 1
                changed = true
            } else if newFlash[p] > 0.01 {
                newFlash[p] *= 0.72
                if newFlash[p] <= 0.01 { newFlash[p] = 0 }
                changed = true
            }
        }
        if changed { flash = newFlash }
    }

    // MARK: Params

    func param(_ id: Int32) -> Float { engine.param(id) }

    func setParam(_ id: Int32, _ value: Float) {
        objectWillChange.send()
        if let sink = paramSink { sink(id, value) } else { engine.setParam(id, value) }
    }

    func binding(_ id: Int32) -> Binding<Float> {
        Binding(get: { [unowned self] in self.param(id) }, set: { [unowned self] newValue in self.setParam(id, newValue) })
    }

    func padBinding(_ pad: Int, _ which: Int32) -> Binding<Float> { binding(DrumEngine.padParam(pad, which)) }

    var isPlaying: Bool {
        get { param(FS_PARAM_PLAY) > 0.5 }
        set { setParam(FS_PARAM_PLAY, newValue ? 1 : 0) }
    }

    var followHost: Bool {
        get { param(FS_PARAM_FOLLOW_HOST) > 0.5 }
        set { setParam(FS_PARAM_FOLLOW_HOST, newValue ? 1 : 0) }
    }

    // MARK: Actions

    func trigger(pad: Int, velocity: UInt8 = 110) {
        engine.trigger(pad: pad, velocity: velocity)
        if selectedPad != pad { selectedPad = pad }
    }

    /// off → normal → ghost → off
    func cycleStep(pad: Int, step: Int) {
        let v = steps[pad][step]
        let next: UInt8 = v == 0 ? 110 : (v > 80 ? 60 : 0)
        engine.setStep(pad: pad, step: step, velocity: next)
        steps[pad][step] = next
    }

    func generateGroove() {
        engine.generatePattern(style: style, density: density)
        objectWillChange.send()
        refreshSteps()
    }

    func clearPattern() {
        engine.clearPattern()
        refreshSteps()
    }

    func load(data: Data, name: String, keepGroove: Bool = false) -> Bool {
        guard engine.load(data: data, keepState: keepGroove) else { return false }
        kitName = name
        refreshFromEngine()
        objectWillChange.send()
        return true
    }
}

enum PadStyle {
    static let colors: [Color] = [
        Color(red: 1.00, green: 0.36, blue: 0.29), // kick
        Color(red: 1.00, green: 0.66, blue: 0.20), // snare
        Color(red: 0.98, green: 0.86, blue: 0.30), // clap
        Color(red: 0.42, green: 0.86, blue: 0.62), // hat
        Color(red: 0.30, green: 0.78, blue: 0.86), // open hat
        Color(red: 0.45, green: 0.55, blue: 1.00), // tom lo
        Color(red: 0.70, green: 0.48, blue: 1.00), // tom hi
        Color(red: 1.00, green: 0.45, blue: 0.75), // perc
    ]
    static func color(_ pad: Int) -> Color { colors[pad % colors.count] }

    static let background = Color(red: 0.07, green: 0.07, blue: 0.09)
    static let panel = Color(red: 0.12, green: 0.12, blue: 0.15)
}
