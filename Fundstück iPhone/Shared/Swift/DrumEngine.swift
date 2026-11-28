import Foundation

/// Thin Swift owner of the C engine. `ref` may be captured by real-time render blocks.
final class DrumEngine {
    let ref: OpaquePointer

    static let padCount = Int(FS_NUM_PADS)
    static let stepCount = Int(FS_NUM_STEPS)

    static let styleNames = ["Boom Bap", "House", "Trap", "Broken"]

    init(sampleRate: Double) {
        ref = fs_engine_create(sampleRate)
    }

    deinit { fs_engine_destroy(ref) }

    func setSampleRate(_ sampleRate: Double) { fs_engine_set_sample_rate(ref, sampleRate) }

    /// Takes ownership of `kit`.
    func load(kit: OpaquePointer, keepState: Bool = false) {
        fs_engine_load_kit(ref, kit, keepState ? 1 : 0)
    }

    @discardableResult
    func load(data: Data, keepState: Bool = false) -> Bool {
        guard let kit = DrumEngine.kit(from: data) else { return false }
        load(kit: kit, keepState: keepState)
        return true
    }

    func loadDemoKit() {
        load(kit: fs_kit_create_demo(44100, UInt32.random(in: 1...UInt32.max)))
    }

    func exportData() -> Data {
        guard let snap = fs_engine_snapshot(ref) else { return Data() }
        defer { fs_kit_destroy(snap) }
        return DrumEngine.data(from: snap)
    }

    static func kit(from data: Data) -> OpaquePointer? {
        data.withUnsafeBytes { raw -> OpaquePointer? in
            guard let base = raw.bindMemory(to: UInt8.self).baseAddress, raw.count > 0 else { return nil }
            return fs_kit_deserialize(base, raw.count)
        }
    }

    static func data(from kit: OpaquePointer) -> Data {
        let size = fs_kit_serialized_size(kit)
        var data = Data(count: size)
        let written = data.withUnsafeMutableBytes { raw -> Int in
            guard let base = raw.bindMemory(to: UInt8.self).baseAddress else { return 0 }
            return fs_kit_serialize(kit, base, size)
        }
        return written == size ? data : Data()
    }

    // MARK: Parameters & pattern

    func param(_ id: Int32) -> Float { fs_engine_get_param(ref, id) }
    func setParam(_ id: Int32, _ value: Float) { fs_engine_set_param(ref, id, value) }

    static func padParam(_ pad: Int, _ which: Int32) -> Int32 {
        FS_PARAM_PAD_BASE + Int32(pad) * 4 + which
    }

    func step(pad: Int, step: Int) -> UInt8 { fs_engine_get_step(ref, Int32(pad), Int32(step)) }
    func setStep(pad: Int, step: Int, velocity: UInt8) { fs_engine_set_step(ref, Int32(pad), Int32(step), velocity) }

    func generatePattern(style: Int, density: Float) {
        fs_engine_generate_pattern(ref, Int32(style), UInt32.random(in: 1...UInt32.max), density)
    }

    func clearPattern() { fs_engine_clear_pattern(ref) }

    func trigger(pad: Int, velocity: UInt8 = 110) { fs_engine_trigger(ref, Int32(pad), velocity) }

    // MARK: Info for UI

    var currentStep: Int { Int(fs_engine_current_step(ref)) }

    func triggerCount(pad: Int) -> UInt32 { fs_engine_trigger_count(ref, Int32(pad)) }

    func padName(_ pad: Int) -> String {
        var buffer = [CChar](repeating: 0, count: 64)
        buffer.withUnsafeMutableBufferPointer { fs_engine_pad_name(ref, Int32(pad), $0.baseAddress, 64) }
        return buffer.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
    }

    func waveform(pad: Int, bins: Int = 48) -> [Float] {
        var out = [Float](repeating: 0, count: bins)
        out.withUnsafeMutableBufferPointer { fs_engine_pad_waveform(ref, Int32(pad), $0.baseAddress, Int32(bins)) }
        return out
    }

    static func midiNote(pad: Int) -> Int { Int(fs_engine_note_for_pad(Int32(pad))) }

    /// Writes every pad as 24-bit WAV into a fresh temp folder and returns the file URLs.
    func exportWAVs(kitName: String) -> [URL] {
        guard let snap = fs_engine_snapshot(ref) else { return [] }
        defer { fs_kit_destroy(snap) }
        let safe = KitStore.sanitize(kitName)
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("Fundstueck-\(safe)", isDirectory: true)
        try? FileManager.default.removeItem(at: dir)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        var urls: [URL] = []
        for pad in 0..<DrumEngine.padCount {
            let name = String(cString: fs_kit_pad_name(snap, Int32(pad)))
            let url = dir.appendingPathComponent(String(format: "%02ld %@ - %@.wav", pad + 1, name, safe))
            if fs_kit_write_pad_wav(snap, Int32(pad), url.path) == 1 { urls.append(url) }
        }
        return urls
    }
}
