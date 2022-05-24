#!/usr/bin/env python3

import argparse, logging
from parser import PuzzleParser as Parser
from manager import HeapRebuildManager as Manager

def _get_parser():
  parser = argparse.ArgumentParser(description='Refine raw heap recordings to puzzles.')
  parser.add_argument('file', \
    help='The raw file to be parsed. ') # type=argparse.FileType('r'),
  parser.add_argument('-s', '--split', dest='split', action="store_true", \
    help="Split the different PIDs within one recording.")
  parser.add_argument('--binary', '-b', metavar='executable', default=None, \
    help='Manually set the binary executable for DWARF info. ', dest='bin') # type=argparse.FileType('r'),
  parser.add_argument('-v', '--debug', dest='debug', type=int, default=logging.WARNING, \
    help='Print verbose/debug information. ')
  parser.add_argument('-l', '--logfile', dest='logfile', \
    help='Write logs to a given file.')
  parser.add_argument('-d', metavar='dir', default=None, \
    help='relative or absolute directory for the target file when splitting.', dest='dir')
  parser.add_argument('-D','--dwarf', dest="dwarf", action="store_true", \
    help="Use DWARF information to attempt and extract variable names (can be slow)")
  parser.add_argument('-c','--cleanup-init', dest="cleanup_init", action="store_true", \
    help="Simplifies the initialisation. WARNING: EXPERIMENTAL!")
  return parser

def main(args):
  argparser = _get_parser()
  settings = vars(argparser.parse_args(args))
  if settings['split']:
      from split import RecordPidSplitter
      rps = RecordPidSplitter(settings['file'], \
        verbose=(settings['debug']<logging.WARNING), \
        new_filelocation=settings['dir'])
      rps.handle()
      return
  manager = Manager(**settings)
  settings["mgr"] = manager
  parser = Parser(**settings)
  # Note: the puzzleparser logger is created and formatted in the manager
  logger = logging.getLogger("puzzleparser.argparser")
  logger.debug(settings)
  if settings["debug"]<40:
      print("{}\nOn file: {}\n{}".format("-"*20, settings["file"], "-"*20))
  parser.parse()
  manager.postprocess()
  if settings['debug']<logging.WARNING:
    print("-------- PARSED --------")
    for op in manager.ops:
      print("{}: ".format(op.name))
      for step in op:
        print("\t{}".format(step))
    print("------- Groups: --------")
    print(manager.groups)
    print("-----END PARSED DATA----")
    print("------- Export code: ---")
  print(manager.export())
  return




if __name__ == "__main__":
  import sys
  main(sys.argv[1:])
