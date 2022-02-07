import os
import subprocess

def main():
    for filename in os.listdir("shaders"):
        f = os.path.join("shaders", filename)
        if os.path.isfile(f):
            if f[len(f)-4:len(f)] != ".spv":
                cmd = "glslc " + str(f) + " -o " + f[:len(f) - 5] + ".spv"
                subprocess.call(cmd)

if __name__ == "__main__":
    main()