#!/usr/bin/env python
#
# This scripts converts oval shaped through-hole mounts to edge cuts
# as per with macrofab requirements:
#   https://macrofab.com/blog/plated-slots-stop-fitting-square-pegs-round-holes/
#   https://macrofab.com/knowledgebase/cutouts-slots-and-routes/

import sys
import re

def thruhole_to_edgecut(m):
  # Add two pads on each side of the board
  # TODO: handle one sided pads with different layers
  m = m.groupdict()
  for k in m:
    if k[-2:] in ('_x', '_y', '_r'):
      m[k] = float(m[k])

  if m['drill_x'] < 0.7 or m['drill_y'] < 0.7:
    print('Macrofab expects slot width to be greater than 0.7mm', file=sys.stderr)
    exit(1)

  # TODO: Some checks because we can't handle all cases right now
  if m['at_r'] not in (0, 90, 180, 270):
    print('cannot handle angles other than 0, 90, 180, 270', file=sys.stderr)
    exit(1)

  width = min(m['drill_x'], m['drill_y'])
  length = max(m['drill_x'], m['drill_y'])
  line_len = length - width
  if m['size_x'] > m['size_y']:
    m['l1_start_x'] = m['l2_start_x'] = -line_len / 2
    m['l1_end_x'] = m['l2_end_x'] = -m['l1_start_x']
    m['l1_start_y'] = m['l1_end_y'] = -width / 2
    m['l2_start_y'] = m['l2_end_y'] = width / 2
    m['a1_start_x'] = m['a1_end_x'] = -line_len / 2
    m['a1_start_y'] = 0
    m['a1_end_y'] = width / 2
    m['a1_angle'] = -180
    m['a2_start_x'] = m['a2_end_x'] = line_len / 2
    m['a2_start_y'] = 0
    m['a2_end_y'] = width / 2
    m['a2_angle'] = 180
  else:
    m['l1_start_y'] = m['l2_start_y'] = -line_len / 2
    m['l1_end_y'] = m['l2_end_y'] = -m['l1_start_y']
    m['l1_start_x'] = m['l1_end_x'] = -width / 2
    m['l2_start_x'] = m['l2_end_x'] = width / 2
    m['a1_start_y'] = m['a1_end_y'] = -line_len / 2
    m['a1_start_x'] = 0
    m['a1_end_x'] = width / 2
    m['a1_angle'] = -180
    m['a2_start_y'] = m['a2_end_y'] = line_len / 2
    m['a2_start_x'] = 0
    m['a2_end_x'] = width / 2
    m['a2_angle'] = 180

  for k in m:
    if k[:3] in ('a1_', 'a2_', 'l1_', 'l2_') and k[-2:] in ('_x', '_y'):
      m[k] = m[k] + m['at' + k[-2:]]

  m['top_layers'] = "(layers F.Cu F.Paste F.Mask)"
  m['bottom_layers'] = "(layers B.Cu B.Paste B.Mask)"
  m['at'] = "(at {at_x} {at_y} {at_r})".format(**m)
  m['size'] = "(size {size_x} {size_y})".format(**m)
  m['net'] = "(net {net_no} {net_name})".format(**m)
  m['slot_common'] = '(layer Edge.Cuts) (width 0.0508)'
  items = [
    '(pad {pad} smd oval {at} {size} {top_layers} {net})',
    '(pad {pad} smd oval {at} {size} {bottom_layers} {net})',
    '(fp_arc (start {a1_start_x} {a1_start_y}) (end {a1_end_x} {a1_end_y}) (angle {a1_angle}) {slot_common})',
    '(fp_arc (start {a2_start_x} {a2_start_y}) (end {a2_end_x} {a2_end_y}) (angle {a2_angle}) {slot_common})',
    '(fp_line (start {l1_start_x} {l1_start_y}) (end {l1_end_x} {l1_end_y}) {slot_common})',
    '(fp_line (start {l2_start_x} {l2_start_y}) (end {l2_end_x} {l2_end_y}) {slot_common})',
  ]
  return ''.join(i.format(**m) + '\n' for i in items)

def main():
  if '-h' in sys.argv[1:]:
    print("Usage: %s < board.kicad_pcb > board-fab.kicad_pcb" % sys.argv[0])
    return
  pat = (
    r'\(pad (?P<pad>\w+) thru_hole oval'
    r' \(at (?P<at_x>[\d.-]+) (?P<at_y>[\d.-]+) (?P<at_r>[\d.-]+)\)'
    r' \(size (?P<size_x>[\d.-]+) (?P<size_y>[\d.-]+)\)'
    r' \(drill oval (?P<drill_x>[\d.-]+) (?P<drill_y>[\d.-]+)\)'
    r' \(layers (?P<layers>[^)]+)\)'
    r' \(net (?P<net_no>\d+) (?P<net_name>[^)]+)\)\)'
  ).replace(' ', r'\s+')
  sys.stdout.write(
    re.sub(pat, thruhole_to_edgecut, sys.stdin.read(), re.M, re.S))

if __name__ == '__main__':
  main()
