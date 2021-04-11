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
  country_code = find_country_code_by_name(country)
  return requests.get(
    'https://digitalapi.auspost.com.au/postage/letter/international/calculate',
    headers={'auth-key': config['auspost_api_key']},
    params={'country_code': country_code, 'service_code': 'INT_LETTER_AIR_OWN_PACKAGING_LIGHT'},
  ).json()['postage_result']['costs']['cost']['cost']

def gen_sub(row, extra_fields={}):
  fields = {
    **extra_fields,
    "first_name": row["First Name"],
    "address_1": row["First Name"] + " " + row["Last Name"],
    "address_2": row["Street"],
    "address_3": row["City"],
    "address_4": row["State / Province"] + ", " + row["Postal/Zip Code"],
    "address_5": row["Country"],
    "phone": row["Phone"],
    "email": row["Email"],
    "invoice": row["Order ID"],
    "c": BOARD_COST,
    "tc": BOARD_COST * int(row["Quantity"]),
    "w": BOARD_WEIGHT,
    "tw": BOARD_WEIGHT * int(row["Quantity"]),
    "q": int(row["Quantity"]),
    "date": time.strftime("%Y-%m-%d"),
    "shipping_cost": get_shipping_cost_for_country(row["Country"]),
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
    if order["Country"].lower() == 'australia' or order["Shipping Method"] != "Australia Post Letter":
      print("Skipping local/tracked item", file=sys.stderr)
      continue
    path = os.path.join(config['output_dir'], "fpx_order_" + order["Order ID"] + ".svg")
    with open(path, 'w') as f:
      f.write(field_pat.sub(gen_sub(order, config['extra_fields']), tpl))
  
if __name__ == '__main__':
  main()
