#!/usr/bin/env python

import csv
import os
import json
import re
import sys
import time
import requests

BOARD_COST = 20 # USD
BOARD_WEIGHT = 3 # grams including packaging

def find_country_code_by_name(name):
  cache = getattr(find_country_code_by_name, '_cache', None)
  if not cache:
    cache = {
      c['name'].lower(): c['code']
      for c in requests.get(
        'https://digitalapi.auspost.com.au/postage/country.json',
        headers={'auth-key': config['auspost_api_key']},
      ).json()['countries']['country']
    }
    cache['united states of america'] = cache['united states']
    find_country_code_by_name._cache = cache
  return find_country_code_by_name._cache[name.lower()]

def get_shipping_cost_for_country(country):
  if country == 'Australia':
    return requests.get(
      'https://digitalapi.auspost.com.au/postage/letter/domestic/calculate.json',
      headers={'auth-key': config['auspost_api_key']},
      params={'weight': '60', 'service_code': 'AUS_LETTER_REGULAR_LARGE_125'},
    ).json()['postage_result']['costs']['cost']['cost']
  else:
    country_code = find_country_code_by_name(country)
    return requests.get(
      'https://digitalapi.auspost.com.au/postage/letter/international/calculate',
      headers={'auth-key': config['auspost_api_key']},
      params={'country_code': country_code, 'service_code': 'INT_LETTER_AIR_OWN_PACKAGING_LIGHT'},
    ).json()['postage_result']['costs']['cost']['cost']

def address_lines_for_order(order):
  lines = {
    "address_1": order["First Name"] + " " + order["Last Name"],
    "address_2": order["Street"],
  }
  if order['Country'] == 'Australia':
    lines.update({
      "address_3": f'{order["City"]} {order["State/Province"]} {order["Postal/Zip Code"]}'.upper(),
      'address_4': '',
      'address_5': '',
    })
  else:
    lines.update({
      "address_3": order["City"],
      "address_4": order["State/Province"] + ", " + order["Postal/Zip Code"],
      "address_5": order["Country"],
    })
  return lines

def gen_sub(order, extra_fields={}):
  fields = {
    **extra_fields,
    **address_lines_for_order(order),
    "first_name": order["First Name"],
    "phone": order["Phone"],
    "email": order["Email"],
    "invoice": order["Order ID"],
    "c": BOARD_COST,
    "tc": BOARD_COST * int(order["Quantity"]),
    "w": BOARD_WEIGHT,
    "tw": BOARD_WEIGHT * int(order["Quantity"]),
    "q": int(order["Quantity"]),
    "date": time.strftime("%Y-%m-%d"),
    "shipping_cost": get_shipping_cost_for_country(order["Country"]),
    "customs_decl_style": "display:none" if order['Country'] == 'Australia' else '',
  }
  return lambda m: str(fields[m.group(1)])

def main():
  global config

  with open('.config.json', 'r') as f:
    config = json.load(f)

  with open('envelope.svg', 'r') as f:
    tpl = f.read()

  field_pat = re.compile(r'\$([^$]+)\$')

  for order in csv.DictReader(sys.stdin):
    if "Australia Post Letter" not in order["Shipping Method"]:
      print("Skipping parcel item %s" % order["Order ID"], file=sys.stderr)
      continue
    if int(order["Quantity"]) > 5:
      print("Order %s has more than 5 items - skipping" % order["Order ID"], file=sys.stderr)
      continue
    path = os.path.join(config['output_dir'], "fpx_order_" + order["Order ID"] + ".svg")
    with open(path, 'w') as f:
      f.write(field_pat.sub(gen_sub(order, config['extra_fields']), tpl))
  
if __name__ == '__main__':
  main()
