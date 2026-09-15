import hashlib
import io
import unittest

from brookesia_usb.protocol import (
    BinaryFrame,
    FrameType,
    PROTOCOL_VERSION,
    decode_frame,
    encode_frame,
    iter_file_frames,
)


class ProtocolTests(unittest.TestCase):
    def test_protocol_version_remains_one(self):
        self.assertEqual(PROTOCOL_VERSION, 1)

    def test_round_trip_data_frame(self):
        frame = BinaryFrame(FrameType.DATA, 7, 3, b"hello")
        self.assertEqual(decode_frame(encode_frame(frame)), frame)

    def test_crc_rejects_payload_corruption(self):
        encoded = bytearray(encode_frame(BinaryFrame(FrameType.DATA, 1, 0, b"payload")))
        encoded[-1] ^= 0x01
        with self.assertRaisesRegex(ValueError, "payload crc"):
            decode_frame(bytes(encoded))

    def test_crc_rejects_header_corruption(self):
        encoded = bytearray(encode_frame(BinaryFrame(FrameType.DATA, 1, 0, b"payload")))
        encoded[8] ^= 0x01
        with self.assertRaisesRegex(ValueError, "header crc"):
            decode_frame(bytes(encoded))

    def test_end_has_no_payload(self):
        encoded = encode_frame(BinaryFrame(FrameType.END, 1, 2))
        self.assertEqual(decode_frame(encoded).frame_type, FrameType.END)

    def test_file_frames_preserve_content(self):
        content = b"a" * 10000 + b"b" * 3
        frames = list(iter_file_frames(io.BytesIO(content), 9, 1024))
        payload = b"".join(decode_frame(frame).payload for frame in frames[:-1])
        self.assertEqual(payload, content)
        self.assertEqual(hashlib.sha256(payload).hexdigest(), hashlib.sha256(content).hexdigest())
        self.assertEqual(decode_frame(frames[-1]).frame_type, FrameType.END)


if __name__ == "__main__":
    unittest.main()
