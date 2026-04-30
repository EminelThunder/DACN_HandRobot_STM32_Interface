from stl import mesh
import numpy as np
import os

STL_DIR = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\STL_parts"
PARTS = ["BASE","Part1","BASE2","BASE2A","BASE2B","ARM1","ARM1A","ARM2","ARM2A","GRIPPER1","GRIPPER2"]

print(f"{'Part':<12} {'MinX':>8} {'MaxX':>8} {'MinY':>8} {'MaxY':>8} {'MinZ':>8} {'MaxZ':>8}   {'CX':>7} {'CY':>7} {'CZ':>7}")
print("-"*100)
for name in PARTS:
    for ext in [".stl", ".STL"]:
        path = os.path.join(STL_DIR, name + ext)
        if os.path.exists(path):
            m = mesh.Mesh.from_file(path)
            v = m.vectors.reshape(-1, 3)
            mn, mx = v.min(0), v.max(0)
            cx, cy, cz = (mn + mx) / 2
            print(f"{name:<12} {mn[0]:>8.1f} {mx[0]:>8.1f} {mn[1]:>8.1f} {mx[1]:>8.1f} {mn[2]:>8.1f} {mx[2]:>8.1f}   {cx:>7.1f} {cy:>7.1f} {cz:>7.1f}")
            break
