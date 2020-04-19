fpx runs on ATtiny 816 which is programmed over a single wire UPDI
interface. It can be programmed and live debugged using Atmel ICE.

fpx exposes UPDI, along with output of the 3.3v linear regulator as test
points. Together with the ground terminal, it's all that's needed to
program and debug the board using Atmel ICE.

![fpx board bottom](./fpx_back_updi.svg)

## Programming Jig

A very rudimentary jig is provided in [STEP](./fpx_rev66_prog_jig.step)
and [STL](./fpx_rev66_prog_jig.stl) formats, suitable for pogo pins with
body diameter of 1 mm and compressed length of 14.5 mm. The source model
is [a public onshape document](https://cad.onshape.com/documents/f66b699467fac012dcd77158/w/a7b99849e050d5e84077c871/e/efc53d333298cbe839427995) which you can copy and make
modifications to. You will need a free onshape account to make changes.
The model can be 3D printed using extrusion printers such as Prusa i3,
without the need for overhang support.

![technical drawing of the programming jig](./fpx_rev66_prog_jig.svg)

The body of pogo pins are exposed on the side of the jig. You will need
clamp-type test probes or similar to make connections to the programmer
as shown below:

![jig connected to Atmel ICE](hookup.png)
