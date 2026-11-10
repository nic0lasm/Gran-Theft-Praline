// Puts the game's recorded sound (-RecordAudio, a WAV) under the video made
// by frames-to-movie.swift, with the system frameworks only.
//
// Usage: swift tools/audio/mux-audio.swift <video.mov> <audio.wav> <output.mov> [audio_offset_s]
//   audio_offset_s > 0 skips the start of the audio (it began before the video).

import AVFoundation
import Foundation

func fail(_ message: String) -> Never {
    FileHandle.standardError.write("mux-audio: \(message)\n".data(using: .utf8)!)
    exit(1)
}

let args = CommandLine.arguments
guard args.count == 4 || args.count == 5 else {
    fail("usage: mux-audio.swift <video.mov> <audio.wav> <output.mov> [audio_offset_s]")
}
let videoAsset = AVURLAsset(url: URL(fileURLWithPath: args[1]))
let audioAsset = AVURLAsset(url: URL(fileURLWithPath: args[2]))
let outputURL = URL(fileURLWithPath: args[3])
let offset = args.count == 5 ? (Double(args[4]) ?? 0) : 0

guard let videoTrack = videoAsset.tracks(withMediaType: .video).first else { fail("no video track in \(args[1])") }
guard let audioTrack = audioAsset.tracks(withMediaType: .audio).first else { fail("no audio track in \(args[2])") }

let composition = AVMutableComposition()
let duration = videoAsset.duration
do {
    let v = composition.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
    try v.insertTimeRange(CMTimeRange(start: .zero, duration: duration), of: videoTrack, at: .zero)
    let a = composition.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
    let start = CMTime(seconds: max(0, offset), preferredTimescale: 48000)
    let available = CMTimeSubtract(audioAsset.duration, start)
    let length = CMTimeMinimum(duration, available)
    try a.insertTimeRange(CMTimeRange(start: start, duration: length), of: audioTrack, at: .zero)
} catch {
    fail("cannot compose: \(error)")
}

try? FileManager.default.removeItem(at: outputURL)
guard let export = AVAssetExportSession(asset: composition, presetName: AVAssetExportPresetHighestQuality) else {
    fail("no export session")
}
export.outputURL = outputURL
export.outputFileType = .mov
let done = DispatchSemaphore(value: 0)
export.exportAsynchronously { done.signal() }
done.wait()
guard export.status == .completed else { fail("export failed: \(String(describing: export.error))") }
print("mux-audio: \(outputURL.path)")
