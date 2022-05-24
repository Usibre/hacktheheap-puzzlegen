#!/usr/bin/env python3

from enum import Enum
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
import subprocess
import re
import parse
from collections import namedtuple
import linecache, logging


LineInfo = namedtuple("LineInfo", ("fileloc", "filename", "lineno", "addr"))

class DWARF:

  def __init__(self, filename = None):
    self.verbose_scinf = False
    self.logger = logging.getLogger("puzzleparser.dwarf")
    self.skip_external = True
    self.statement_only = False
    self.linelist = []
    self.filename = filename
    self.handlers = []
    self.main_aslr_offset = None
    self.base_addresses = []
    self.base_map = {}
    self.elffile = None
    self.dwarfinfo = None
    self.dwarfinfos = []
    self._name_gen = _name_generator()
    self.current_base = 0x0
    self.process(filename)

  def __del__(self):
    try:
      for f in self.handlers:
        f.close()
    except AttributeError:
      pass

  def dbg(self, x):
    self.logger.debug(x)

  def reset(self):
    self.base_addresses = []
    self.base_map = {}
    self.main_aslr_offset = None

  def set_main(self, main_addr):
    dwarf_main_addr = self.get_main_addr()
    if dwarf_main_addr is None:
      self.dbg("Cannot determine ASLR offset without binary info!")
      return 0
    aslr_offset = main_addr - dwarf_main_addr
    if aslr_offset not in self.base_addresses:
      self.base_addresses.append(aslr_offset)
      self.base_map[aslr_offset] = "main"
    self.main_aslr_offset = aslr_offset
    self.dbg("Aslr offset is: 0x{:x}".format(aslr_offset))
    return aslr_offset

  def skip(self, addr):
    if not self.skip_external:
      return False
    return self.main_aslr_offset!=self.get_base_addr(addr)


  def get_name(self, ret_addr):
    if ret_addr <= 0:
      x = next(self._name_gen)
    else:
      x = self._get_name(ret_addr)
    self.dbg("\tName chosen: {}".format(x))
    return x

  def get_name_of_none(self, ret_addr):
    viable = []
    static_addr = self.rt_to_static_addr(ret_addr)
    sc_names = self.get_prev_lines_from_addr(static_addr)
    for line in sc_names:
      if line is None:
        continue
      line = line.split('=')[0].strip().lower()
      line = re.sub(r'[^a-z]+', '', line)
      if len(line)>2 and len(line) < 20:
        viable.append(line)
    if len(viable)<1:
      return None
    result = max(viable, key = lambda x : len(x))
    self.dbg("\tName chosen: {:s}".format(result))
    return result


  def _get_name(self, ret_addr):
    viable = []
    static_addr = self.rt_to_static_addr(ret_addr)
    sc_names = self.get_prev_lines_from_addr(static_addr)
    for line in sc_names:
      if line is None:
        continue
      line = line.split('=')[0].strip().lower()
      line = re.sub(r'[^a-z]+', '', line)
      if len(line)>2 and len(line) < 20:
        viable.append(line)
    if self.debug or (self.verbose_scinf and len(viable)>0):
      self.dbg("DEBUG DEBUG: ret addr = 0x{:x}".format(ret_addr))
      self.dbg("Scanned on obj {:s} (base 0x{:x}).".format(self.get_libname(ret_addr), self.get_base_addr(ret_addr)))
      self.dbg("{:d} viable names from dwarf.".format(len(viable)))
      for v in viable:
        self.dbg("\t{:s}".format(v))
    if len(viable)<1:
      return next(self._name_gen)
    #if len(viable)==1:
    #  return viable[0]
    result = max(viable, key = lambda x : len(x))
    self.dbg("\tName chosen: {:s}".format(result))
    return result


  def rt_to_static_addr(self, addr):
    return addr - self.get_base_addr(addr)


# internal
  def get_libname(self, addr):
    try:
      base_addr = self.get_base_addr(addr)
      if base_addr in self.base_map:
        return self.base_map[self.get_base_addr(addr)]
    except ValueError:
      return "NOBASE"
    except IndexError:
      return "NONE"
    if base_addr <= 0:
      return "NOBASE"
    return "NONE"

  def get_base_addr(self, addr, print_filename = False):
    corresponding_base_address = 0x0
    for b in self.base_addresses:
      if b < addr and b > corresponding_base_address:
        corresponding_base_address = b
        break
    self.current_base = corresponding_base_address
    return corresponding_base_address

  def _find_abs_path(self, rel_path, base_addr = 0x0):
    if rel_path is None:
      return None;
    if rel_path.startswith("/"):
      return rel_path
    if len(self.dwarfinfos) < 1:
      return None
    if rel_path.startswith("./"):
      rel_path = rel_path[2:]
    for dinfo in self.dwarfinfos:
      for CU in dinfo.iter_CUs():
        location = str(CU.get_top_DIE().get_full_path())
        if (location.endswith(rel_path)):
          return location
    return None

  def _is_external_file(self, abs_path):
    if abs_path is None:
      return True
    if self.dwarfinfo is None:
      return None
    for CU in self.dwarfinfo.iter_CUs():
      if CU.get_top_DIE().get_full_path() == abs_path:
        return False
    return True

  def get_line_from_lineinf(self, line_inf):
    if line_inf == None:
      return None
    abs_location = self._find_abs_path(line_inf.fileloc, self.current_base)
    if abs_location is None:
      return None
    self.dbg("Potential source location: {} : {}".format(abs_location, line_inf.lineno))
    return linecache.getline(abs_location, line_inf.lineno)

  def get_line_from_addr(self, addr):
    return self.get_line_from_lineinf(self.get_lineinf(addr))

  def get_lines_from_addr(self, addr):
    return [self.get_line_from_lineinf(x) for x in self.get_linesinf(addr)]

  def get_next_line_from_addr(self, addr):
    return self.get_line_from_lineinf(self.get_next_lineinf(addr))

  def get_next_lines_from_addr(self, addr):
    return [self.get_line_from_lineinf(x) for x in self.get_next_linesinf(addr)]

  def get_prev_line_from_addr(self, addr):
    return self.get_line_from_lineinf(self.get_prev_lineinf(addr))

  def get_prev_lines_from_addr(self, addr):
    y = self.get_prev_linesinf(addr)
    if self.verbose_scinf and len(y)>0:
      print("\tFound {} lineinfs".format(len(y)))
    return [self.get_line_from_lineinf(x) for x in y]

  def get_prev_lineinf(self, ret_addr):
    return self.get_lineinf(ret_addr-1)
    for i in range(1, len(self.linelist)):
      if self.linelist[i].addr >= ret_addr:
        return self.linelist[i-1]
    return self.linelist[len(self.linelist)-1]

  def get_prev_linesinf(self, addr):
    res = []
    for i in range(len(self.linelist)):
      if self.linelist[i].addr >= addr:
        i-=1
        prev_addr = self.linelist[i].addr
        while (self.linelist[i].addr == prev_addr):
          res.append(self.linelist[i])
          i-=1
        return res
      else:
        pass
    return res

  def get_linesinf(self, addr):
    res = []
    for i in range(len(self.linelist)):
      if self.linelist[i].addr > addr:
        i-=1
        prev_addr = self.linelist[i].addr
        while (self.linelist[i].addr == prev_addr):
          res.append(self.linelist[i])
          i-=1
        return res
    return res

  def get_lineinf(self, addr):
    for i in range(len(self.linelist)):
      if self.linelist[i].addr > addr:
        return self.linelist[i-1]
    return self.linelist[len(self.linelist)-1]

  def get_next_lineinf(self, addr):
    for lineinf in self.linelist:
      if lineinf.addr > addr:
        return lineinf
    return None

  def get_next_linesinf(self, addr):
    res = []
    for i in range(len(self.linelist)):
      if self.linelist[i].addr > addr:
        curr_addr = self.linelist[i].addr
        while (self.linelist[i].addr == curr_addr):
          res.append(self.linelist[i])
          i+=1
        return res
    return res

  def get_main_addr(self):
    if self.elffile is not None:
      for section in self.elffile.iter_sections():
        if isinstance(section, SymbolTableSection):
          main_opts = section.get_symbol_by_name('main')
          if (main_opts is not None and len(main_opts) >= 1):
            return main_opts[0]['st_value']
    # lets try the same thing for dwarf if elf fails smh
    if self.dwarfinfo is None:
      return None
    for CU in self.dwarfinfo.iter_CUs():
      for die in CU.iter_DIEs():
        #sth
        if die.tag == "DW_TAG_subprogram" and 'DW_AT_name' in die.attributes and \
            die.attributes['DW_AT_name'].value == b'main':
          return die.attributes['DW_AT_low_pc'].value
    return None

  def add_dynamic(self, filename, base_addr = 0x0):
      if base_addr not in self.base_addresses:
        self.base_addresses.append(base_addr)
        self.base_map[base_addr] = filename
      self.process(filename)

  def process(self, filename):
    if filename is None:
      return
    for f in self.handlers:
      if f.name == filename:
        return
      self.dbg("Reading {}, this might take a while.".format(filename))
    try:
      f = open(filename, 'rb')
      elffile = ELFFile(f)
      if not elffile.has_dwarf_info():
        f.close()
        self.dbg("File '{}' has no dwarf.".format(filename))
        return
      self.handlers.append(f)
      dwarfinfo = elffile.get_dwarf_info()
      self.dwarfinfos.append(dwarfinfo)
      if (filename == self.filename):
        self.dwarfinfo = dwarfinfo
        self.elffile = elffile
      self.parse_debug_lines(filename)
      self.dbg("We have {} line numbers.".format(len(self.linelist)))
      assert(len(self.linelist)<1 or sorted(self.linelist, key=(lambda x: x.addr)))
    except (OSError, IOError) as e:
      self.dbg("Opening file '{}' failed.".format(filename))
      return


  def parse_debug_lines(self, filename):
    self.dbg("Using console for: '{}'".format("readelf --debug-dump=decodedline {}".format(filename)))
    result = subprocess.run(['readelf', '--debug-dump=decodedline', filename], stdout=subprocess.PIPE)
    lines = result.stdout.decode('utf-8', errors='ignore').splitlines(False);
    if len(lines) < 2:
      self.dbg("No debug info found on '{}'.".format(filename))
      return
    tmp = parse.parse("CU: {}:", lines[2])
    current_file = tmp[0]
    if not lines[0].startswith("Contents"):
      self.dbg("PRETTY BAD WARNING! readelf tool unreliable!")
    if self.statement_only:
      readelf_regex = re.compile('^([^\s]+)\s+([0-9]+)\s+(0x[0-9a-f]+)\s+[0-9]*\s*x')
    else:
      readelf_regex = re.compile('^([^\s]+)\s+([0-9]+)\s+(0x[0-9a-f]+)')
    for i in range(4, len(lines)):
      res = readelf_regex.search(lines[i])
      if res is None:
        tmp = parse.search("CU: {}:", lines[i])
        if tmp is not None:
          current_file = tmp[0]
        else:
          tmp = parse.search("{}:", lines[i])
          if tmp is not None:
            current_file = tmp[0]
        if tmp is None and lines[i].strip() != "" and not lines[i].startswith("File name"):
          self.dbg("Failed to parse: {}".format(lines[i]))
          continue
      else:
        if self.skip_external and self._is_external_file(self._find_abs_path(current_file, self.current_base)):
          continue
        self.linelist.append(LineInfo(current_file, res.group(1), int(res.group(2),base=10), int(res.group(3), base=16)))
    # done filling linelist
    self.linelist.sort(key = lambda x: x.addr)






def _name_generator():
  ctr = 0
  while True:
    i = ctr
    newCharacter = i % 26
    i //= 26
    s = "" +chr(newCharacter + ord('A') )
    while i != 0:
      newCharacter = i % 26
      i //= 26
      s = chr(newCharacter + ord('A') ) + s
    yield s
    ctr += 1

def main(args):
  filename = args[1]
  static_stuff = DWARF(filename)

  print("Previous line at 0x{:x}:\t".format(0x3393) + static_stuff.get_prev_line_from_addr(0x3393))
  print("Line at binary address 0x{:x}:\t".format(0x3393) + static_stuff.get_line_from_addr(0x3393))
  print("Next line at 0x{:x}:\t\t".format(0x3393) + static_stuff.get_next_line_from_addr(0x3393))

if __name__ == "__main__":
  import sys
  main(sys.argv)
