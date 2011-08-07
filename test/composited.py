from kaa import imlib2, display, main
from kaa.display import x11
decorated = False
composite = False

def redraw(regions):
    window.render_imlib2_image(image)

def key_pressed(key):
    key = key.lower()
    print key
    if key == 'esc' or key == 'q':
        main.stop()
    
    if key == 'd':
        global decorated
        decorated = not decorated
        window.hide()
        window.set_decorated(decorated)
        window.show()
    
def mapped():
    window.focus()

if x11.get_display().composite_supported():
    composite = True
else:
    print 'Compositing not supported on this display!'
    sys.exit(1)

window = display.X11Window(size = (800, 600), title = "Kaa Display Test",composite=composite)

imlib2.add_font_path("data")

image = imlib2.new((800,600))
image.clear()
image.draw_ellipse((400,300), (400, 300), (0,0,255,128))
image.draw_text((10, 50), "This is a Kaa Display Composited Window Test", (255,255,255,255), "VeraBd/24")

window.signals['expose_event'].connect(redraw)
window.signals['key_press_event'].connect(key_pressed)
window.signals['map_event'].connect(mapped)

window.set_decorated(decorated)
window.show()


print 'Shaped window test app'
print 'Use the following keys to test features'
print 'Esc or q = Quit'
print 'd = Toggle Decorated'
main.run()
