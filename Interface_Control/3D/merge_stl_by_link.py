# ============================================================
# Script gộp các STL riêng lẻ thành 7 file theo khâu robot
# Cách chạy (PowerShell):
#   pip install numpy-stl
#   python merge_stl_by_link.py
# ============================================================

from stl import mesh
import numpy as np
import os

STL_DIR = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\STL_parts"
OUT_DIR = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\assets\models"
os.makedirs(OUT_DIR, exist_ok=True)

# ---------------------------------------------------------------
# Nhóm file theo khâu robot RV-M1
# Nếu sau khi xem robot.CATProduct trong FreeCAD thấy nhóm sai,
# hãy điều chỉnh lại danh sách bên dưới.
# ---------------------------------------------------------------
LINK_GROUPS = {
    "link0": [
        "BASE"                                
    ],
    "link1": [
        "BASE2", "BASE2A", "BASE2B"
    ],
    "link2": [
        "ARM1", "ARM1A"
    ],
    "link3": [
        "ARM2", "ARM2A"
    ],
    "link4": [
        "GRIPPER1", "GRIPPER2"
    ],
}

def merge_stl(names, output_path):
    meshes = []
    for name in names:
        path = os.path.join(STL_DIR, name + ".stl")
        if os.path.exists(path):
            meshes.append(mesh.Mesh.from_file(path))
            print(f"    + {name}.stl")
        else:
            print(f"    [CANH BAO] Khong tim thay: {name}.stl")

    if not meshes:
        print(f"    [BO QUA] Khong co file nao!")
        return

    combined = mesh.Mesh(np.concatenate([m.data for m in meshes]))
    combined.save(output_path)
    print(f"  -> Da luu: {os.path.basename(output_path)}\n")


print("=== Bat dau gop STL theo khau robot ===\n")
for link_name, parts in LINK_GROUPS.items():
    print(f"[{link_name}]")
    merge_stl(parts, os.path.join(OUT_DIR, link_name + ".stl"))

print("=== XONG! Kiem tra thu muc: assets/models/ ===")
print("Cac file output:")
for f in os.listdir(OUT_DIR):
    size_kb = os.path.getsize(os.path.join(OUT_DIR, f)) // 1024
    print(f"  {f}  ({size_kb} KB)")
