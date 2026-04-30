# ============================================================
# Script chạy bằng FreeCADCmd.exe (headless mode)
# Cách chạy trong PowerShell:
#   & "C:\Program Files\FreeCAD 1.1\bin\FreeCADCmd.exe" "...\convert_to_stl.py"
# ============================================================
# Dùng Part.read() thay vì FreeCAD.open() để đọc đúng định dạng CATIA

import FreeCAD
import Part
import Mesh
import MeshPart
import os

input_dir  = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D"
output_dir = r"e:\DACN_HandRoBot\Eminel\DACN_HandRobot_STM32\Interface_Control\3D\STL_parts"

os.makedirs(output_dir, exist_ok=True)

cat_files = [f for f in os.listdir(input_dir) if f.endswith('.CATPart')]
print(f"Tim thay {len(cat_files)} file CATPart.\n")

for i, filename in enumerate(cat_files, 1):
    filepath = os.path.join(input_dir, filename)
    print(f"[{i}/{len(cat_files)}] Dang xu ly: {filename} ...")

    try:
        # Dùng Part.read() - đọc trực tiếp qua OpenCASCADE, hỗ trợ CATIA V5
        shape = Part.read(filepath)

        if shape.isNull():
            print(f"  -> Shape rong, bo qua.")
            continue

        # Convert shape -> mesh -> STL
        doc = FreeCAD.newDocument("temp")
        part_obj = doc.addObject("Part::Feature", "Shape")
        part_obj.Shape = shape
        doc.recompute()

        # Tạo mesh từ shape
        mesh = MeshPart.meshFromShape(
            Shape=shape,
            LinearDeflection=0.1,   # Độ phân giải: nhỏ = mịn hơn
            AngularDeflection=0.1
        )

        stl_name = filename.replace('.CATPart', '.stl')
        stl_path = os.path.join(output_dir, stl_name)
        mesh.write(stl_path)
        print(f"  -> Da luu: {stl_name} ({mesh.CountFacets} triangles)")

        FreeCAD.closeDocument(doc.Name)

    except Exception as e:
        print(f"  -> LOI: {e}")

print("\n=== Hoan thanh! Kiem tra thu muc STL_parts/ ===")
