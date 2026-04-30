# ============================================================
# Script tách file STEP assembly -> 7 file STL theo khâu robot
#
# Cách dùng:
#   1. Trong SolidWorks: mở robot.CATProduct -> File -> Save As
#      -> chọn "STEP AP214 (*.step)" -> lưu tên robot_assembly.step vào thư mục 3D\
#   2. Chạy script này: python step_to_stl.py
#
# Requires: pip install cadquery
# ============================================================

import cadquery as cq
import os

STEP_FILE = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\robot_assembly.step"
OUT_DIR   = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\assets\models"

os.makedirs(OUT_DIR, exist_ok=True)

print(f"Dang doc file STEP: {STEP_FILE}")
print("(Co the mat vai phut...)\n")

# Đọc toàn bộ assembly STEP
assembly = cq.importers.importStep(STEP_FILE)

# Lấy tất cả solids riêng lẻ
solids = assembly.solids().vals()
print(f"Tim thay {len(solids)} solid trong assembly.\n")

# Xuất toàn bộ assembly thành 1 STL để kiểm tra
full_stl = os.path.join(OUT_DIR, "robot_full.stl")
cq.exporters.export(assembly, full_stl)
print(f"Da xuat toan bo robot: robot_full.stl")
print(f"  -> Kiem tra file nay truoc de xac nhan STEP load dung.\n")

# Xuất từng solid riêng lẻ (để xác nhận nhóm khâu)
parts_dir = os.path.join(OUT_DIR, "parts")
os.makedirs(parts_dir, exist_ok=True)

for i, solid in enumerate(solids):
    part = cq.Workplane().newObject([solid])
    fname = os.path.join(parts_dir, f"part_{i+1:03d}.stl")
    cq.exporters.export(part, fname)
    bb = solid.BoundingBox()
    vol = solid.Volume()
    print(f"  part_{i+1:03d}.stl  vol={vol:.0f}mm3  "
          f"size=({bb.xmax-bb.xmin:.0f} x {bb.ymax-bb.ymin:.0f} x {bb.zmax-bb.zmin:.0f})mm")

print(f"\nDa xuat {len(solids)} parts vao: {parts_dir}")
print("\nBuoc tiep theo:")
print("  1. Mo cac file part_XXX.stl trong MeshLab hoac Windows 3D Viewer")
print("  2. Xac dinh part nao thuoc khau nao")
print("  3. Sua bien LINK_GROUPS trong merge_stl_by_link.py")
print("  4. Chay: python merge_stl_by_link.py")
