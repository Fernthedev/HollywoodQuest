import os
import shutil

shader_folder = "./shaders"

shader_header_folder = "./include/shaders"

if os.path.exists(shader_header_folder):
    print("Clearing shader header folder!")
    shutil.rmtree(shader_header_folder)

os.makedirs(shader_header_folder)

def shader_file(shader_name, shader_contents):
    lines = ""

    for line in shader_contents:
        line = line.replace("\n", "")
        # If we do this, the shader lines won't be as expected.
        # if line.strip() == "":
        #     continue
        lines += f"\"{line}\\n\"\n"

    return f"""
constexpr const char* {shader_name.replace(".","_")} = {lines};
"""


for shader_name in os.listdir(shader_folder):
    file_path = os.path.join(shader_folder, shader_name)
    print(f"Making header {shader_name}.hpp in {shader_header_folder}")

    with open(f"{file_path}", "r") as original_shader_file:
        with open(f"{shader_header_folder}/{shader_name}.hpp", "w") as file_converted:
            shader_header_code = shader_file(shader_name, original_shader_file.readlines())
            file_converted.write(shader_header_code)

print("Done making shaders!")