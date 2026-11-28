import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @EnvironmentObject var audio: AudioController

    @State private var showImporter = false
    @State private var showKits = false
    @State private var showSave = false
    @State private var saveName = ""
    @State private var shareItems: [URL] = []
    @State private var showShare = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 14) {
                    RecordSection(audio: audio, showImporter: $showImporter)
                    KitEditorView(model: audio.kit)
                    if !KitStore.isSharedContainerAvailable {
                        Text("Hinweis: App Group nicht eingerichtet – das AUv3-Plugin sieht deine Kits erst, wenn die App Group in beiden Targets aktiv ist.")
                            .font(.caption2)
                            .foregroundColor(.orange)
                    }
                }
                .padding(12)
            }
            .background(PadStyle.background.ignoresSafeArea())
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) { KitTitle(model: audio.kit) }
                ToolbarItemGroup(placement: .navigationBarLeading) {
                    Button { showKits = true } label: { Image(systemName: "square.stack.3d.up") }
                        .accessibilityLabel("Kits")
                }
                ToolbarItemGroup(placement: .navigationBarTrailing) {
                    Button {
                        shareItems = audio.exportWAVs()
                        showShare = !shareItems.isEmpty
                    } label: { Image(systemName: "square.and.arrow.up") }
                        .accessibilityLabel("WAVs exportieren")
                    Button {
                        saveName = audio.kit.kitName == "Demo-Kit" ? KitStore.uniqueName() : audio.kit.kitName
                        showSave = true
                    } label: { Image(systemName: "square.and.arrow.down") }
                        .accessibilityLabel("Kit speichern")
                }
            }
            .toolbarBackground(PadStyle.background, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
        }
        .preferredColorScheme(.dark)
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.audio]) { result in
            if case .success(let url) = result { audio.importAudio(from: url) }
        }
        .sheet(isPresented: $showKits) {
            KitListView { entry in audio.load(entry); showKits = false }
        }
        .sheet(isPresented: $showShare) { ActivityView(items: shareItems) }
        .alert("Kit speichern", isPresented: $showSave) {
            TextField("Name", text: $saveName)
            Button("Speichern") { audio.saveKit(name: saveName) }
            Button("Abbrechen", role: .cancel) {}
        }
        .overlay(alignment: .bottom) {
            if let msg = audio.message {
                Text(msg)
                    .font(.footnote.weight(.medium))
                    .foregroundColor(.white)
                    .padding(.horizontal, 14).padding(.vertical, 10)
                    .background(Capsule().fill(Color.black.opacity(0.85)))
                    .padding(.bottom, 20)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                    .onTapGesture { audio.message = nil }
                    .task(id: msg) {
                        try? await Task.sleep(nanoseconds: 3_500_000_000)
                        if audio.message == msg { withAnimation { audio.message = nil } }
                    }
            }
        }
        .animation(.easeOut, value: audio.message)
    }
}

struct KitTitle: View {
    @ObservedObject var model: KitModel
    var body: some View {
        VStack(spacing: 0) {
            Text("Fundstück").font(.system(size: 10, weight: .heavy, design: .rounded)).foregroundColor(.pink)
            Text(model.kitName).font(.system(size: 15, weight: .bold, design: .rounded)).foregroundColor(.white)
        }
    }
}

struct RecordSection: View {
    @ObservedObject var audio: AudioController
    @Binding var showImporter: Bool

    var body: some View {
        VStack(spacing: 10) {
            HStack(spacing: 12) {
                Button { audio.toggleRecording() } label: {
                    ZStack {
                        Circle().fill(audio.isRecording ? Color.red : Color.red.opacity(0.25))
                        Circle().stroke(Color.red, lineWidth: 2)
                        Image(systemName: audio.isRecording ? "stop.fill" : "mic.fill")
                            .font(.system(size: 22, weight: .bold))
                            .foregroundColor(.white)
                    }
                    .frame(width: 60, height: 60)
                    .scaleEffect(audio.isRecording ? 1 + CGFloat(audio.inputLevel) * 0.15 : 1)
                }
                .accessibilityLabel(audio.isRecording ? "Aufnahme stoppen" : "Aufnahme starten")

                VStack(alignment: .leading, spacing: 4) {
                    Text(audio.isRecording ? "Aufnahme läuft … klopf, klatsch, beatbox!" :
                            (audio.hasRecording ? "Aufnahme: \(String(format: "%.1f", audio.recordingSeconds)) s" : "Nimm Geräusche auf – alles wird zu Drums"))
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundColor(.white)
                    RecordingWaveform(levels: audio.overview, markers: audio.sliceMarkers, live: audio.isRecording)
                        .frame(height: 40)
                }
            }

            HStack(spacing: 4) {
                Knob(title: "Empfindl.", value: $audio.sensitivity, range: 0...1, color: .red,
                     format: { String(format: "%.0f%%", $0 * 100) })
                Knob(title: "Charakter", value: $audio.character, range: 0...1, color: .pink,
                     format: { $0 < 0.33 ? "roh" : ($0 < 0.7 ? "punchy" : "fett") })
                Spacer(minLength: 4)
                VStack(spacing: 6) {
                    HStack(spacing: 6) {
                        Button { audio.buildKit(newSeed: false) } label: {
                            Label("Kit bauen", systemImage: "wand.and.stars")
                        }
                        .buttonStyle(PillStyle(color: .pink))
                        Button { audio.buildKit(newSeed: true) } label: {
                            Image(systemName: "shuffle")
                        }
                        .buttonStyle(PillStyle(color: .purple))
                        .accessibilityLabel("Neu würfeln")
                    }
                    .disabled(!audio.hasRecording || audio.isBuilding || audio.isRecording)
                    .opacity(audio.hasRecording ? 1 : 0.4)
                    HStack(spacing: 6) {
                        Button { showImporter = true } label: { Label("Datei", systemImage: "folder") }
                            .buttonStyle(PillStyle(color: .gray))
                        Toggle(isOn: $audio.keepGroove) { Text("Groove halten").font(.system(size: 10)) }
                            .toggleStyle(.button)
                            .tint(.orange)
                            .font(.system(size: 11))
                    }
                }
            }
            if audio.isBuilding {
                ProgressView("Analysiere …").font(.caption).tint(.white).foregroundColor(.white)
            }
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 16).fill(PadStyle.panel))
    }
}

struct RecordingWaveform: View {
    let levels: [Float]
    let markers: [Double]
    let live: Bool

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 6).fill(Color.black.opacity(0.35))
                let shown = live ? Array(levels.suffix(160)) : levels
                WaveformShape(samples: shown)
                    .fill(live ? Color.red.opacity(0.8) : Color.white.opacity(0.55))
                    .padding(.vertical, 3)
                if !live {
                    ForEach(markers.indices, id: \.self) { i in
                        Rectangle()
                            .fill(PadStyle.color(i))
                            .frame(width: 1.5)
                            .offset(x: geo.size.width * CGFloat(markers[i]))
                    }
                }
            }
        }
    }
}

struct PillStyle: ButtonStyle {
    var color: Color
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 13, weight: .semibold))
            .foregroundColor(.white)
            .padding(.horizontal, 12).padding(.vertical, 8)
            .background(Capsule().fill(color.opacity(configuration.isPressed ? 0.5 : 0.75)))
    }
}

struct KitListView: View {
    var onSelect: (KitStore.Entry) -> Void
    @State private var entries = KitStore.list()
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                if entries.isEmpty {
                    Text("Noch keine Kits gespeichert.").foregroundColor(.secondary)
                }
                ForEach(entries) { entry in
                    Button { onSelect(entry) } label: {
                        VStack(alignment: .leading) {
                            Text(entry.name).font(.headline)
                            Text(entry.date, style: .date).font(.caption).foregroundColor(.secondary)
                        }
                    }
                }
                .onDelete { idx in
                    idx.map { entries[$0] }.forEach(KitStore.delete)
                    entries = KitStore.list()
                }
            }
            .navigationTitle("Meine Kits")
            .toolbar { Button("Fertig") { dismiss() } }
        }
        .preferredColorScheme(.dark)
    }
}

struct ActivityView: UIViewControllerRepresentable {
    let items: [Any]
    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: nil)
    }
    func updateUIViewController(_ vc: UIActivityViewController, context: Context) {}
}
