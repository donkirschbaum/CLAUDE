import SwiftUI

@main
struct FundstueckApp: App {
    @StateObject private var audio = AudioController()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(audio)
        }
    }
}
