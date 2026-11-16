#!/usr/bin/env python3
"""Writes lantern.gltf (data URIs) and lantern.glb: a locally authored glTF material fixture.

Parts (node order): bronze base (color factor), painted body (embedded PNG base color texture),
flame (emissive with KHR_materials_emissive_strength), and glass cap (blend alpha and a normal
texture, which Swan reports as unsupported). Run: python3 assets/models/make_lantern.py
"""
import base64
import json
import struct
import sys
import zlib
from pathlib import Path


def png(width, height, pixel):
    rows = b''.join(b'\0' + b''.join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    header = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')


def cube():
    faces = [((1, 0, 0), (0, 0, -1)), ((-1, 0, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0)),
             ((0, -1, 0), (1, 0, 0)), ((0, 0, 1), (1, 0, 0)), ((0, 0, -1), (-1, 0, 0))]
    positions, normals, uvs, indices = [], [], [], []
    for normal, tangent in faces:
        n, t = normal, tangent
        b = (n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0])
        base = len(positions)
        for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)):
            su, sv = 2 * u - 1, 2 * v - 1
            positions.append(tuple(n[i] + su * t[i] + sv * b[i] for i in range(3)))
            normals.append(n)
            uvs.append((u, 1 - v))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, uvs, indices


def build():
    positions, normals, uvs, indices = cube()
    geometry = (b''.join(struct.pack('<3f', *p) for p in positions) + b''.join(struct.pack('<3f', *n) for n in normals)
                + b''.join(struct.pack('<2f', *t) for t in uvs) + b''.join(struct.pack('<H', i) for i in indices))
    checker = png(16, 16, lambda x, y: (200, 40, 32, 255) if (x // 4 + y // 4) % 2 else (240, 225, 190, 255))
    flat_normal = png(1, 1, lambda x, y: (128, 128, 255, 255))
    gltf = {
        'asset': {'version': '2.0', 'generator': 'Swan authored material fixture'},
        'extensionsUsed': ['KHR_materials_emissive_strength'],
        'scene': 0, 'scenes': [{'nodes': [0, 1, 2, 3]}],
        'nodes': [
            {'name': 'base', 'mesh': 0, 'translation': [0, 0.1, 0], 'scale': [0.6, 0.1, 0.6]},
            {'name': 'body', 'mesh': 1, 'translation': [0, 0.6, 0], 'scale': [0.4, 0.4, 0.4]},
            {'name': 'flame', 'mesh': 2, 'translation': [0, 1.15, 0], 'scale': [0.12, 0.15, 0.12]},
            {'name': 'cap', 'mesh': 3, 'translation': [0, 1.38, 0], 'scale': [0.5, 0.08, 0.5]},
        ],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}, 'indices': 3, 'material': m}]}
                   for m in range(4)],
        'materials': [
            {'name': 'bronze', 'pbrMetallicRoughness': {'baseColorFactor': [0.55, 0.33, 0.18, 1], 'metallicFactor': 0.8}},
            {'name': 'painted', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0}},
            {'name': 'flame', 'pbrMetallicRoughness': {'baseColorFactor': [1, 0.75, 0.35, 1]}, 'emissiveFactor': [1, 0.6, 0.2],
             'extensions': {'KHR_materials_emissive_strength': {'emissiveStrength': 4}}},
            {'name': 'glass', 'alphaMode': 'BLEND', 'normalTexture': {'index': 1},
             'pbrMetallicRoughness': {'baseColorFactor': [0.6, 0.85, 1, 0.5]}},
        ],
        'textures': [{'source': 0}, {'source': 1}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': 24, 'type': 'VEC3', 'min': [-1, -1, -1], 'max': [1, 1, 1]},
            {'bufferView': 1, 'componentType': 5126, 'count': 24, 'type': 'VEC3'},
            {'bufferView': 2, 'componentType': 5126, 'count': 24, 'type': 'VEC2'},
            {'bufferView': 3, 'componentType': 5123, 'count': 36, 'type': 'SCALAR'},
        ],
    }
    views = [(0, 288), (288, 288), (576, 192), (768, 72)]
    gltf['bufferViews'] = [{'buffer': 0, 'byteOffset': o, 'byteLength': n} for o, n in views]
    return gltf, geometry, [checker, flat_normal]


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent
    gltf, geometry, images = build()
    text = json.loads(json.dumps(gltf))
    text['buffers'] = [{'byteLength': len(geometry), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(geometry).decode()}]
    text['images'] = [{'uri': 'data:image/png;base64,' + base64.b64encode(i).decode()} for i in images]
    (out / 'lantern.gltf').write_text(json.dumps(text, indent=1) + '\n')
    # GLB: geometry, then each PNG in its own 4-byte-aligned buffer view.
    binary = bytearray(geometry)
    images_json = []
    for image in images:
        binary += b'\0' * (-len(binary) % 4)
        gltf['bufferViews'].append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(image)})
        images_json.append({'bufferView': len(gltf['bufferViews']) - 1, 'mimeType': 'image/png'})
        binary += image
    binary += b'\0' * (-len(binary) % 4)
    gltf['images'] = images_json
    gltf['buffers'] = [{'byteLength': len(binary)}]
    header = json.dumps(gltf, separators=(',', ':')).encode()
    header += b' ' * (-len(header) % 4)
    body = struct.pack('<II', len(header), 0x4E4F534A) + header + struct.pack('<II', len(binary), 0x004E4942) + bytes(binary)
    (out / 'lantern.glb').write_bytes(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)


if __name__ == '__main__':
    main()
