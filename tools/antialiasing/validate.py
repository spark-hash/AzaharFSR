# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the actual presentation shaders and optionally dispatch/read back on a GPU."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--validator', default='glslangValidator')
parser.add_argument('--gpu-test', type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
fxaa = (repo / 'externals/fxaa/Fxaa3_11.h').read_text(encoding='latin-1')
smaa = (repo / 'externals/smaa/SMAA.hlsl').read_text(encoding='latin-1')
# Match the compute adaptation in vk_antialiasing.cpp; upstream files stay unmodified.
smaa = smaa.replace('discard;', 'return float2(0.0);')
smaa = smaa.replace('texture(tex, coord)', 'textureLod(tex, coord, 0.0)')
smaa = smaa.replace('textureOffset(tex, coord, offset)', 'textureLodOffset(tex, coord, 0.0, offset)')
with tempfile.TemporaryDirectory(prefix='azahar-aa-') as directory:
    out = Path(directory)
    for name in ['aa_copy', 'fxaa', 'smaa_edges', 'smaa_weights', 'smaa_blend']:
        code = (repo / ('src/video_core/host_shaders/' + name + '.comp')).read_text(encoding='utf-8')
        code = code.replace('// FXAA\n', fxaa + '\n').replace('// SMAA\n', smaa + '\n')
        path = out / (name + '.comp')
        path.write_text(code, encoding='utf-8')
        subprocess.run([args.validator, '-V', str(path), '-o', str(out / (name + '.spv'))], check=True)
    print('All five compute shaders compile for Vulkan')
    if args.gpu_test:
        subprocess.run([str(args.gpu_test.resolve()), str(out)], check=True)
