import SwiftUI

// MARK: - Knob

struct Knob: View {
    let title: String
    @Binding var value: Float
    let range: ClosedRange<Float>
    var color: Color = .white
    var format: (Float) -> String = { String(format: "%.2f", $0) }

    @State private var dragStart: Float?

    private var norm: CGFloat {
        CGFloat((value - range.lowerBound) / (range.upperBound - range.lowerBound))
    }

    var body: some View {
        VStack(spacing: 3) {
            ZStack {
                Circle()
                    .trim(from: 0.125, to: 0.875)
                    .stroke(Color.white.opacity(0.12), style: StrokeStyle(lineWidth: 5, lineCap: .round))
                    .rotationEffect(.degrees(90))
                Circle()
                    .trim(from: 0.125, to: 0.125 + 0.75 * max(0.001, min(1, norm)))
                    .stroke(color, style: StrokeStyle(lineWidth: 5, lineCap: .round))
                    .rotationEffect(.degrees(90))
                Text(format(value))
                    .font(.system(size: 10, weight: .semibold, design: .rounded).monospacedDigit())
                    .foregroundColor(.white.opacity(0.85))
            }
            .frame(width: 46, height: 46)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { g in
                        if dragStart == nil { dragStart = value }
                        let span = range.upperBound - range.lowerBound
                        let delta = Float(-g.translation.height + g.translation.width * 0.5) / 160 * span
                        value = min(range.upperBound, max(range.lowerBound, (dragStart ?? value) + delta))
                    }
                    .onEnded { _ in dragStart = nil }
            )
            .onTapGesture(count: 2) { value = defaultValue }
            Text(title)
                .font(.system(size: 10, weight: .medium))
                .foregroundColor(.white.opacity(0.6))
                .lineLimit(1)
        }
        .frame(minWidth: 54)
        .accessibilityElement()
        .accessibilityLabel(title)
        .accessibilityValue(format(value))
        .accessibilityAdjustableAction { dir in
            let stepSize = (range.upperBound - range.lowerBound) / 20
            switch dir {
            case .increment: value = min(range.upperBound, value + stepSize)
            case .decrement: value = max(range.lowerBound, value - stepSize)
            @unknown default: break
            }
        }
    }

    private var defaultValue: Float {
        range.contains(0) && range.lowerBound < 0 ? 0 : value
    }
}

// MARK: - Waveform

struct WaveformShape: Shape {
    let samples: [Float]
    func path(in rect: CGRect) -> Path {
        var p = Path()
        guard samples.count > 1 else { return p }
        let mid = rect.midY
        let stepX = rect.width / CGFloat(samples.count - 1)
        p.move(to: CGPoint(x: rect.minX, y: mid))
        for (i, s) in samples.enumerated() {
            p.addLine(to: CGPoint(x: rect.minX + CGFloat(i) * stepX, y: mid - CGFloat(s) * rect.height / 2))
        }
        for (i, s) in samples.enumerated().reversed() {
            p.addLine(to: CGPoint(x: rect.minX + CGFloat(i) * stepX, y: mid + CGFloat(s) * rect.height / 2))
        }
        p.closeSubpath()
        return p
    }
}

// MARK: - Pads

struct PadView: View {
    let index: Int
    @ObservedObject var model: KitModel
    @State private var isDown = false

    var body: some View {
        let color = PadStyle.color(index)
        let flash = model.flash[index]
        let selected = model.selectedPad == index
        ZStack(alignment: .topLeading) {
            RoundedRectangle(cornerRadius: 14)
                .fill(color.opacity(0.22 + 0.6 * flash))
            WaveformShape(samples: model.waveforms[index])
                .fill(color.opacity(0.55))
                .padding(.horizontal, 8)
                .padding(.vertical, 22)
            VStack(alignment: .leading, spacing: 0) {
                Text(model.padNames[index])
                    .font(.system(size: 13, weight: .bold, design: .rounded))
                    .foregroundColor(.white)
                Spacer()
                Text(noteName(DrumEngine.midiNote(pad: index)))
                    .font(.system(size: 9, weight: .medium, design: .monospaced))
                    .foregroundColor(.white.opacity(0.5))
            }
            .padding(8)
            RoundedRectangle(cornerRadius: 14)
                .stroke(selected ? Color.white : color.opacity(0.5), lineWidth: selected ? 2 : 1)
        }
        .aspectRatio(1.15, contentMode: .fit)
        .scaleEffect(isDown ? 0.96 : 1)
        .animation(.easeOut(duration: 0.08), value: isDown)
        .contentShape(Rectangle())
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    if !isDown {
                        isDown = true
                        model.trigger(pad: index)
                    }
                }
                .onEnded { _ in isDown = false }
        )
        .accessibilityLabel("Pad \(model.padNames[index])")
        .accessibilityAddTraits(.isButton)
    }

    private func noteName(_ n: Int) -> String {
        let names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        guard n >= 0 else { return "" }
        return "\(names[n % 12])\(n / 12 - 2) · \(n)"
    }
}

struct PadGridView: View {
    @ObservedObject var model: KitModel
    var columns = 4

    var body: some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 8), count: columns), spacing: 8) {
            ForEach(0..<DrumEngine.padCount, id: \.self) { i in
                PadView(index: i, model: model)
            }
        }
    }
}

// MARK: - Pad parameters

struct PadParamsView: View {
    @ObservedObject var model: KitModel

    var body: some View {
        let pad = model.selectedPad
        let color = PadStyle.color(pad)
        HStack(spacing: 6) {
            Text(model.padNames[pad])
                .font(.system(size: 13, weight: .bold, design: .rounded))
                .foregroundColor(color)
                .frame(width: 64, alignment: .leading)
            Knob(title: "Level", value: model.padBinding(pad, FS_PAD_GAIN), range: 0...1.5, color: color,
                 format: { String(format: "%.0f%%", $0 * 100) })
            Knob(title: "Tune", value: model.padBinding(pad, FS_PAD_TUNE), range: -24...24, color: color,
                 format: { String(format: "%+.0f", $0) })
            Knob(title: "Decay", value: model.padBinding(pad, FS_PAD_DECAY), range: 0.05...1, color: color,
                 format: { String(format: "%.0f%%", $0 * 100) })
            Knob(title: "Pan", value: model.padBinding(pad, FS_PAD_PAN), range: -1...1, color: color,
                 format: { $0 < -0.02 ? String(format: "L%.0f", -$0 * 100) : ($0 > 0.02 ? String(format: "R%.0f", $0 * 100) : "C") })
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 14).fill(PadStyle.panel))
    }
}

// MARK: - Step sequencer

struct SequencerView: View {
    @ObservedObject var model: KitModel

    var body: some View {
        VStack(spacing: 4) {
            ForEach(0..<DrumEngine.padCount, id: \.self) { pad in
                HStack(spacing: 3) {
                    Text(model.padNames[pad])
                        .font(.system(size: 10, weight: .semibold, design: .rounded))
                        .foregroundColor(PadStyle.color(pad))
                        .frame(width: 52, alignment: .leading)
                        .lineLimit(1)
                        .onTapGesture { model.trigger(pad: pad) }
                    ForEach(0..<DrumEngine.stepCount, id: \.self) { step in
                        StepCell(velocity: model.steps[pad][step],
                                 color: PadStyle.color(pad),
                                 isCurrent: model.currentStep == step,
                                 isBeat: step % 4 == 0)
                            .onTapGesture { model.cycleStep(pad: pad, step: step) }
                    }
                }
            }
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 14).fill(PadStyle.panel))
    }
}

struct StepCell: View {
    let velocity: UInt8
    let color: Color
    let isCurrent: Bool
    let isBeat: Bool

    var body: some View {
        RoundedRectangle(cornerRadius: 4)
            .fill(velocity > 0 ? color.opacity(velocity > 80 ? 1 : 0.45) : Color.white.opacity(isBeat ? 0.11 : 0.06))
            .overlay(RoundedRectangle(cornerRadius: 4).stroke(Color.white.opacity(isCurrent ? 0.9 : 0), lineWidth: 1.5))
            .frame(minWidth: 12, minHeight: 20)
            .contentShape(Rectangle())
    }
}

// MARK: - Groove controls

struct GrooveControlsView: View {
    @ObservedObject var model: KitModel
    var showFollowHost = false

    var body: some View {
        VStack(spacing: 10) {
            HStack(spacing: 10) {
                Button {
                    model.isPlaying.toggle()
                } label: {
                    Image(systemName: model.isPlaying ? "stop.fill" : "play.fill")
                        .font(.system(size: 18, weight: .bold))
                        .frame(width: 44, height: 36)
                        .background(RoundedRectangle(cornerRadius: 10).fill(model.isPlaying ? Color.red.opacity(0.8) : Color.green.opacity(0.7)))
                        .foregroundColor(.white)
                }
                .accessibilityLabel(model.isPlaying ? "Pattern stoppen" : "Pattern starten")

                Picker("Stil", selection: $model.style) {
                    ForEach(0..<DrumEngine.styleNames.count, id: \.self) { Text(DrumEngine.styleNames[$0]).tag($0) }
                }
                .pickerStyle(.segmented)
            }
            HStack(spacing: 4) {
                Button { model.generateGroove() } label: {
                    Label("Groove", systemImage: "dice.fill")
                        .font(.system(size: 13, weight: .semibold))
                        .padding(.horizontal, 10).padding(.vertical, 8)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.white.opacity(0.12)))
                }
                Button { model.clearPattern() } label: {
                    Image(systemName: "trash")
                        .padding(8)
                        .background(RoundedRectangle(cornerRadius: 10).fill(Color.white.opacity(0.08)))
                }
                .accessibilityLabel("Pattern löschen")
                Spacer(minLength: 4)
                Knob(title: "Dichte", value: $model.density, range: 0...1, color: .orange,
                     format: { String(format: "%.0f%%", $0 * 100) })
                if !(showFollowHost && model.followHost) {
                    Knob(title: "Tempo", value: model.binding(FS_PARAM_TEMPO), range: 60...200, color: .orange,
                         format: { String(format: "%.0f", $0) })
                }
                Knob(title: "Swing", value: model.binding(FS_PARAM_SWING), range: 0...0.5, color: .orange,
                     format: { String(format: "%.0f%%", $0 * 200) })
                Knob(title: "Drive", value: model.binding(FS_PARAM_DRIVE), range: 0...1, color: .red,
                     format: { String(format: "%.0f%%", $0 * 100) })
                Knob(title: "Master", value: model.binding(FS_PARAM_MASTER), range: 0...1.5, color: .white,
                     format: { String(format: "%.0f%%", $0 * 100) })
            }
            .foregroundColor(.white)
            if showFollowHost {
                Toggle("Host-Tempo & Transport folgen", isOn: Binding(get: { model.followHost }, set: { model.followHost = $0 }))
                    .font(.system(size: 12))
                    .foregroundColor(.white.opacity(0.8))
                    .tint(.orange)
            }
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 14).fill(PadStyle.panel))
    }
}

/// Pads + sound shaping + groove, used by the app and the plugin.
struct KitEditorView: View {
    @ObservedObject var model: KitModel
    var isPlugin = false

    var body: some View {
        VStack(spacing: 10) {
            PadGridView(model: model)
            PadParamsView(model: model)
            GrooveControlsView(model: model, showFollowHost: isPlugin)
            SequencerView(model: model)
        }
    }
}
