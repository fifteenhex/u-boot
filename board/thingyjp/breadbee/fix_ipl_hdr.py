import argparse
import struct

VECTORANDMAGICLEN = 8
HDRLEN = 16
LE32FMT = "<I"


def pad(data):
    pad = 4 - (len(data) % 4)
    if pad != 0:
        print("adding %d pad bytes" % pad)
        data = data.ljust(len(data) + pad, b'\x00')
    return data


def calc_chksum(data):
    chksum = 0
    step = struct.calcsize(LE32FMT)
    for i in range(HDRLEN, len(data), step):
        le32 = data[i:i + step]
        n = struct.unpack(LE32FMT, le32)[0]
        #print("%x" % n)
        chksum += n

    chksum = chksum & 0xffffffff
    return chksum


def write_output(data, chksum, output):
    with open(output, "wb") as o:
        # write the reset vector and magic
        o.write(data[0:VECTORANDMAGICLEN])
        # write the data length
        o.write(struct.pack(LE32FMT, len(data)))
        # write the checksum
        o.write(struct.pack(LE32FMT, chksum))
        o.write(data[HDRLEN:])


def fix(input, output):
    print("Fixing %s" % input)
    with open(args.i, "rb") as f:
        data = f.read()
        data = pad(data)
        chksum = calc_chksum(data)
        write_output(data, chksum, output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser("Fix checksum and length on a MStar IPL blob")
    parser.add_argument("-i", help="input file")
    parser.add_argument("-o", help="output file")
    args = parser.parse_args()

    fix(args.i, args.o)
