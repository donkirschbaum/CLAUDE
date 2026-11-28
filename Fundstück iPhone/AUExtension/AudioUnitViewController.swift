import CoreAudioKit
import SwiftUI

/// Principal class of the extension: creates the audio unit and its SwiftUI view.
public class AudioUnitViewController: AUViewController, AUAudioUnitFactory {
    private var audioUnit: FundstueckAudioUnit?
    private var hosting: UIHostingController<PluginRootView>?

    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        let au = try FundstueckAudioUnit(componentDescription: componentDescription, options: [])
        audioUnit = au
        DispatchQueue.main.async { [weak self] in self?.connectViewIfReady() }
        return au
    }

    public override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 0.07, green: 0.07, blue: 0.09, alpha: 1)
        preferredContentSize = CGSize(width: 820, height: 560)
        connectViewIfReady()
    }

    private func connectViewIfReady() {
        guard hosting == nil, isViewLoaded, let au = audioUnit else { return }
        let model = KitModel(engine: au.drum)
        model.kitName = au.kitName
        model.paramSink = { [weak au] address, value in
            if let p = au?.parameterTree?.parameter(withAddress: AUParameterAddress(address)) {
                p.value = value
            } else {
                au?.drum.setParam(address, value)
            }
        }
        au.onKitChanged = { [weak model, weak au] in
            model?.kitName = au?.kitName ?? "Kit"
            model?.refreshFromEngine()
        }
        let host = UIHostingController(rootView: PluginRootView(model: model, audioUnit: au))
        host.view.backgroundColor = .clear
        addChild(host)
        host.view.frame = view.bounds
        host.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(host.view)
        host.didMove(toParent: self)
        hosting = host
    }
}

struct PluginRootView: View {
    @ObservedObject var model: KitModel
    let audioUnit: FundstueckAudioUnit
    @State private var kits: [KitStore.Entry] = []
    @State private var keepGroove = false

    var body: some View {
        ScrollView {
            VStack(spacing: 10) {
                HStack(spacing: 10) {
                    Text("Fundstück")
                        .font(.system(size: 15, weight: .heavy, design: .rounded))
                        .foregroundColor(.pink)
                    Menu {
                        if kits.isEmpty {
                            Text("Keine Kits – baue eins in der Fundstück-App")
                        }
                        ForEach(kits) { entry in
                            Button(entry.name) {
                                if audioUnit.loadKit(entry, keepGroove: keepGroove) {
                                    model.kitName = entry.name
                                    model.refreshFromEngine()
                                }
                            }
                        }
                        Divider()
                        Toggle("Groove beim Laden behalten", isOn: $keepGroove)
                    } label: {
                        HStack(spacing: 4) {
                            Text(model.kitName).font(.system(size: 14, weight: .bold, design: .rounded))
                            Image(systemName: "chevron.down").font(.system(size: 11, weight: .bold))
                        }
                        .foregroundColor(.white)
                        .padding(.horizontal, 10).padding(.vertical, 6)
                        .background(Capsule().fill(Color.white.opacity(0.1)))
                    }
                    Button { kits = KitStore.list() } label: {
                        Image(systemName: "arrow.clockwise").foregroundColor(.white.opacity(0.7))
                    }
                    .accessibilityLabel("Kit-Liste aktualisieren")
                    Spacer()
                    Text("MIDI: C1 Kick · D1 Snare · D#1 Clap · F#1 Hat · A#1 Open")
                        .font(.system(size: 9, design: .monospaced))
                        .foregroundColor(.white.opacity(0.4))
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                }
                KitEditorView(model: model, isPlugin: true)
            }
            .padding(10)
        }
        .background(PadStyle.background)
        .preferredColorScheme(.dark)
        .onAppear { kits = KitStore.list() }
    }
}
