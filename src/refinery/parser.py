import logging, re, parse
import operation
from dwarf import DWARF

class PuzzleParser:
    callback_strlen = 4
    def __init__(self, **kwargs):
        self.logger = logging.getLogger("puzzleparser.parser")
        self._set_callback()
        self.binfile = "" if "binary" not in kwargs else kwargs["binary"]
        if "file" not in kwargs:
            raise RuntimeError("No file to parse!")
        self.parsefilename = kwargs["file"]
        self.mgr = HeapRebuildManager(**kwargs) if "mgr" not in kwargs else kwargs["mgr"]
        self._prepared = False
        self._additional_attr = []
        self.dwarf = DWARF()
        self.use_dwarf = False if "dwarf" not in kwargs else kwargs["dwarf"]

    def _prepare(self):
        if self._prepared:
            return
        self.f = open(self.parsefilename, "r", encoding="utf-8", errors="ignore")
        self.mgr.reset()
        self.op = self.mgr.new_op("init", isInit=True)
        self._saved_name = ""
        self._prepared = True


    def _set_callback(self):
        self.callback = {}
        self.cbty = {}
        self.cb_re = re.compile("^[A-Za-z_]+")
        # meta stuff
        self.callback["FORK"] = self._parse_fork
        self.callback["PROCESS_START"] = self._parse_newexec
        self.callback["DYNAMIC"] = self._parse_dynlib
        self.callback["START"] = self._nop
        self.callback["END"] = self._nop
        self.callback["NEXT"] = self._parse_next
        self.callback["SET"] = self._parse_setting
        self.callback["NAME"] = self._parse_name
        self.callback["BUGGED"] = self._parse_bugged
        self.callback["TARGET"] = self._parse_target
        # opsteptys
        self.cbty["MALLOC"] = operation.MallocStep
        self.cbty["CALLOC"] = operation.CallocStep
        self.cbty["MEMALIGN"] = operation.MemalignStep
        self.cbty["REALLOC"] = operation.ReallocStep
        self.cbty["FREE"] = operation.FreeStep
        self.cbty["MALLOPT"] = operation.MalloptStep

    def _nextline(self):
        for line in self.f:
            x = line.strip().split(" ")
            yield " ".join(x)

    def parse(self):
        self._prepare()
        for line in self._nextline():
            line = line.strip()
            if line.startswith("#"):
                continue
            self._parse_line(line)
        self.mgr.clean()
        self.f.close()
        self._prepared = False

    def _parse_line(self, line):
        try:
            self.logger.debug("Parsing line: {}".format(line))
            linety = self.cb_re.search(line).group(0).upper()
            if linety in self.cbty:
                try:
                    opstep = self.cbty[linety].from_string(line)
                    if self._saved_name is not None and len(self._saved_name)>0:
                        opstep.add("name", self._saved_name)
                        self._saved_name = None
                    elif self.use_dwarf and self.dwarf is not None and "ret_addr" in opstep:
                        name = self.dwarf.get_name_of_none(opstep["ret_addr"])
                        if name is not None:
                            opstep.add("name", name)
                    self.op.add(opstep)
                    for i in range(len(self._additional_attr)-1,-1,-1):
                        ctr = self._additional_attr[i][0][0]
                        attr = self._additional_attr[i][0][1]
                        val = self._additional_attr[i][1]
                        del self._additional_attr[i]
                        if ctr <= 1:
                            self.op[-1][attr]=val
                        else:
                            self._additional_attr.append(((ctr-1,attr),val))
                except ValueError as e:
                    self.logger.warning("Could not parse line? (linety: {}) (error: {})".format(linety, str(e)))
                    if re.search("@ [0-9]+$", line):
                        self.logger.warning("You may need to split first (-s).")
                    else:
                        self.logger.warning("\t'{:s}'".format(line))
            else:
                self.callback[linety](line)
        except (KeyError, AttributeError) as e:
            self.logger.warning("Don't understand this line: '{}'".format(line))
            self.logger.debug(e)



    """
    Individual Parsing functions.
    """
    def _parse_newexec(self, line):
        try:
            if self.use_dwarf and self.dwarf is not None:
                self.dwarf.reset()
            main_addr = parse.parse("PROCESS_START 0x{:x}", line).fixed[0]
            self.mgr.new_exec(main_addr)
            if self.use_dwarf and self.dwarf is not None:
                self.dwarf.set_main(main_addr)
            self._parse_next("NEXT init")
        except AttributeError:
            # No main addr given
            self.mgr.new_exec(0x0)

    def _parse_fork(self, line):
        self.logger.critical("Run the splitter first!")
        raise NotImplementedError("Not splitted.")

    def _parse_dynlib(self, line):
        splitted = line.split(' ')
        self.mgr.add_dynamic_library(splitted[1], int(splitted[3], 16))
        if self.use_dwarf and self.dwarf is not None:
            self.dwarf.add_dynamic(splitted[1], int(splitted[3], 16))

    def _nop(self, line):
        pass

    def _parse_next(self, line):
        try:
            name = parse.parse("NEXT {}", line).fixed[0]
        except AttributeError:
            name = "anon" #self.dwarf.get_name(0)
        self.logger.debug("\tNext operation: {}".format(name))
        self.op = self.mgr.new_op(name)

    def _parse_setting(self, line):
        splitted = line.split(" ")
        param = splitted[1].lower()
        val = " ".join(splitted[2:]).lower()
        if "target" == param:
            self.binfile = val
            if self.use_dwarf:
                self.dwarf = DWARF(val)
            return
        elif "allocator" == param or "alloc" == param:
            self.mgr.set_operation_mode(val)
        elif "attack" == param:
            self.mgr.set_attack_mode(val)
        elif "size" == param or "heapsize"==param:
            try:
                self.mgr.heapsize = int(val)
            except ValueError:
                pass
        elif "" == param:
            return
        else:
            self.logger.warning("Unknown set option: {:s}".format(param))
            return
        return

    def _parse_name(self, line):
        try:
            name = parse.parse("NAME {}", line).fixed[0]
        except AttributeError:
            name = None #self.dwarf.get_name(0)
        self._saved_name = name

    def _parse_bugged(self, line):
        try:
            number = parse.parse("BUGGED {:d}",line).fixed[0]
        except AttributeError:
            number = 1
        self._additional_attr.append(((number,"bty"), 'B'))

    def _parse_target(self, line):
        try:
            number = parse.parse("TARGET {:d}",line).fixed[0]
        except AttributeError:
            number = 1
        self._additional_attr.append(((number,"bty"), 'T'))

    # Deprecated:
    def _parse_malloc(self, line):
        self.op.add(MallocStep.from_string(line))
    def _parse_calloc(self, line):
        self.op.add(CallocStep.from_string(line))
    def _parse_memalign(self, line):
        self.op.add(MemalignStep.from_string(line))
    def _parse_realloc(self, line):
        self.op.add(ReallocStep.from_string(line))
    def _parse_free(self, line):
        self.op.add(FreeStep.from_string(line))
    def _parse_mallopt(self, line):
        self.op.add(MalloptStep.from_string(line))





if __name__ == "__main__":
    print("Testing... ")
    p = PuzzleParser()
    p.parse_line("Hello, world!")
    p.parse_line("MALLOC Hello, world!")
