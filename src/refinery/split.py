#!/usr/bin/env python3

import parse, os

class RecordPidSplitter:
    def __init__(self, filename, new_filelocation = None, new_filename = None, verbose = False):
        self.verbose = verbose
        self.filename = filename
        self.outfile = new_filename 
        self.outloc = new_filelocation
        self.f = open(self.filename, "r", encoding="utf8", errors="ignore")
        self.split = {}
        self.split[0] = []

    def handle(self):
        for line in self._nextline():
            self._parse_line(line)
        self.f.close()
        basename = self.filename.split('/')[-1]
        if self.verbose: 
            print("Found {} different processes.".format(len(self.split)-1))
        for key in self.split:
            if key == 0:
                continue
            if self.outfile is not None: 
                basename = self.outfile
            file_to_open = "{}.{}".format(str(key), basename) 
            if self.outloc is not None: 
                file_to_open = os.path.join(self.outloc, file_to_open)
            if self.verbose: 
                print("Saving file to {}".format(file_to_open))
            f = open(file_to_open, "w", encoding="utf8")
            f.write("\n".join(self.split[key]))
            f.close()
        if self.verbose: 
            print("Done!")

    def _parse_line(self, line):
        if self.verbose: 
            print("\t{}".format(line))
        if line.startswith("FORK"):
            (parent, child, pid) = parse.parse("FORK {:d} -> {:d} @ {:d}", line)
            assert(child not in self.split)
            if pid not in self.split:
                self.split[pid] = self.split[0].copy()
            self.split[child] = self.split[parent].copy()
            return
            ## etc
        if line.startswith("DYNAMIC"):
            # count the occurences of @ becauase dynamic uses the symbol for location, not just pid 
            if line.count("@") == 1:
                for key in self.split:
                    self.split[key].append(line)
                return
        try:
            (effective_line, pid) = parse.parse("{} @ {:d}", line)
            if pid == 0:
                raise AttributeError
            if pid not in self.split:
                self.split[pid] = self.split[0].copy()
            self.split[pid].append(effective_line)
        except AttributeError as e:
            # no pid, add to all
            if self.verbose: 
                print("\t\tAdding to all.")
            (effective_line, _) = parse.parse("{} @ {:d}", line)
            for key in self.split:
                self.split[key].append(effective_line)
        except TypeError as e: 
            # parsing failed: no pid mentioned 
            for key in self.split:
                self.split[key].append(line)
        return


    def _nextline(self):
        for line in self.f:
            x = line.strip().split(" ")
            x[0] = x[0]#.upper()
            yield " ".join(x)



if __name__ == "__main__":
    import argparse, sys
    argparser = argparse.ArgumentParser(description='Split raw heap recordings according to pid.')
    argparser.add_argument('file', metavar='File', help='The raw file to be splitted. ')
    argparser.add_argument('-d', metavar='dir', default=None, \
            help='relative or absolute directory for the target file.', dest='dir')
    argparser.add_argument('-o', metavar='out', default=None, \
            help='name for the file (becomes [pid].[name].txt)', dest='outname')
    argparser.add_argument('-v', '--debug', dest='debug', action='store_true', \
    help='Print verbose/debug information. ')
    settings = argparser.parse_args(sys.argv[1:])
    x = RecordPidSplitter(settings.file, new_filelocation=settings.dir, new_filename=settings.outname, \
            verbose=settings.debug)
    x.handle()
