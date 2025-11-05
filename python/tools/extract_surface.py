import numpy as np
from plyfile import PlyData, PlyElement
from skimage import measure
from skimage.filters import threshold_otsu


def main():
    vol = np.fromfile('output.raw', dtype=np.float32)
    vol = vol.reshape((800, 800, 800))
    thresh = threshold_otsu(vol) * 1.5
    verts, faces, normals, values = measure.marching_cubes(vol, thresh)
    verts = verts * 0.0439
    vertex = [(x, y, z) for x, y, z in verts]
    vertex = np.array(vertex, dtype=[('x', 'f4'), ('y', 'f4'), ('z', 'f4')])
    v_el = PlyElement.describe(vertex, 'vertex')
    indices = [([i, j, k],) for i, j, k in faces]
    indices = np.array(indices, dtype=[('vertex_indices', 'i4', (3,))])
    f_el = PlyElement.describe(indices, 'face')
    PlyData([v_el, f_el]).write('surface.ply')


if __name__ == '__main__':
    main()
