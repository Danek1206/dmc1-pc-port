import struct

size = 256

# Icon directory entry
icon_dir = struct.pack('<HHH', 0, 1, 1)

# For 256x256, width/height = 0 in icon format
# 8 fields: width, height, colors, reserved, planes, bpp, size, offset
icon_entry = struct.pack('<BBBBHHII', 0, 0, 0, 0, 1, 1, size*size*4, 22)

# BMP info header - 12 fields
bmp_header = struct.pack('<IiiHHIIiiII',
    40, size, size, 1, 32, 0, size*size*4, 2835, 2835, 0, 0
)

with open('icon.ico', 'wb') as f:
    f.write(icon_dir + icon_entry + bmp_header)
    
    for y in range(size):
        for x in range(size):
            r = int(40 + (x/size)*20)
            g = int(20 + (y/size)*10)
            b = int(60 + ((size-x)/size)*30)
            a = 255
            f.write(struct.pack('BBBB', b, g, r, a))

import os
print(f'icon.ico created! Size: {os.path.getsize("icon.ico")} bytes')
