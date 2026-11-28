import Foundation

/// Kits are stored in the shared App Group container, so the AUv3 plugin sees
/// everything you build in the app.
enum KitStore {
    /// ⚠️ Must match the App Group in both .entitlements files (and your Apple Developer account).
    static let appGroup = "group.com.example.fundstueck"

    struct Entry: Identifiable, Hashable {
        let url: URL
        let name: String
        let date: Date
        var id: URL { url }
    }

    static var isSharedContainerAvailable: Bool {
        FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroup) != nil
    }

    static var directory: URL {
        let fm = FileManager.default
        let base = fm.containerURL(forSecurityApplicationGroupIdentifier: appGroup)
            ?? fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let dir = base.appendingPathComponent("Kits", isDirectory: true)
        try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    static func sanitize(_ name: String) -> String {
        let bad = CharacterSet(charactersIn: "/\\:?%*|\"<>")
        let cleaned = name.components(separatedBy: bad).joined(separator: "-").trimmingCharacters(in: .whitespaces)
        return cleaned.isEmpty ? "Kit" : String(cleaned.prefix(60))
    }

    static func list() -> [Entry] {
        let fm = FileManager.default
        guard let urls = try? fm.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.contentModificationDateKey]) else { return [] }
        return urls.filter { $0.pathExtension == "fskit" }.map { url in
            let date = (try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            return Entry(url: url, name: url.deletingPathExtension().lastPathComponent, date: date)
        }.sorted { $0.date > $1.date }
    }

    @discardableResult
    static func save(_ data: Data, name: String) throws -> URL {
        let url = directory.appendingPathComponent(sanitize(name)).appendingPathExtension("fskit")
        try data.write(to: url, options: .atomic)
        return url
    }

    static func load(_ entry: Entry) -> Data? { try? Data(contentsOf: entry.url) }

    static func delete(_ entry: Entry) { try? FileManager.default.removeItem(at: entry.url) }

    static func latest() -> Data? { list().first.flatMap(load) }

    static func uniqueName(base: String = "Kit") -> String {
        let names = Set(list().map(\.name))
        var i = names.count + 1
        while names.contains("\(base) \(i)") { i += 1 }
        return "\(base) \(i)"
    }
}
