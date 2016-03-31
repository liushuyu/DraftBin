from PIL import Image, ImageEnhance
#import ImageEnhance
import pytesseract
import sys,os
from utils import *

def dec(fn):
    #for si in sys.argv[1:]:
        si = fn
        im = Image.open(si)
        nx, ny = im.size
        im2 = im.resize((int(nx*5), int(ny*5)), Image.BICUBIC)
        enh = ImageEnhance.Contrast(im2)
        enh.enhance(77.7)
        im2 = im2.convert("L")
        im2.save("temp2.png")
        imgx = Image.open("temp2.png")
        imgx = imgx.convert("RGBA")
        pix = imgx.load()

        for y in range(imgx.size[1]):
            for x in range(imgx.size[0]):
                if pix[x, y][0] < 90:
                    pix[x, y] = (0, 0, 0, 255)

        for y in range(imgx.size[1]):
            for x in range(imgx.size[0]):
                if pix[x, y][1] < 136:
                    pix[x, y] = (0, 0, 0, 255)

        for y in range(imgx.size[1]):
            for x in range(imgx.size[0]):
                if pix[x, y][2] > 0:
                    pix[x, y] = (255, 255, 255, 255)

        imgx.resize((132, 54), Image.CUBIC)
        imgx.save('temp3.png')
        image = Image.open('temp3.png')
        print (pytesseract.pytesseract.image_to_string(image,lang='eng'))

    
def get_lmms_sc():
    sc_dat=get_content('https://lmms.io/lsp/get_image.php', decoded=False)
    tmp_file = open('a.png','wb')
    tmp_file.write(sc_dat)
    tmp_file.close()
    dec('a.png')
    os.remove('a.png')
    
    
if __name__ == '__main__':
    get_lmms_sc()


#Deprecated:
'''
imgx = Image.open('temp2.png')
imgx = imgx.convert("RGBA")
pix = imgx.load()
for y in range(imgx.size[1]):
    for x in range(imgx.size[0]):
        if pix[x, y] != (0, 0, 0, 255):
            pix[x, y] = (255, 255, 255, 255)
imgx.save("bw.gif", "GIF")
original = Image.open('bw.gif')
bg = original.resize((132, 54), Image.NEAREST)
ext = ".png"
bg.save("temp3" + ext)
'''
'''
imgx = Image.open("temp2.png")
imgx = imgx.convert("RGBA")
pix = imgx.load()

for y in range(imgx.size[1]):
    for x in range(imgx.size[0]):
        if pix[x, y][0] < 90:
            pix[x, y] = (0, 0, 0, 255)

for y in range(imgx.size[1]):
    for x in range(imgx.size[0]):
        if pix[x, y][1] < 136:
            pix[x, y] = (0, 0, 0, 255)

for y in range(imgx.size[1]):
    for x in range(imgx.size[0]):
        if pix[x, y][2] > 0:
            pix[x, y] = (255, 255, 255, 255)

imgx.resize((132, 54), Image.CUBIC)
imgx.save('temp3.png')
'''
