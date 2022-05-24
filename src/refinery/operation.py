import parse, re, random
class Operation:
    def __init__(self, mgr, name=None, isInit=False):
        self.steps = []
        self.mgr = mgr
        self.name = name
        self.isInit = isInit
    def add(self, step):
        if not isinstance(step, OpStep):
            return False
        self.steps.append(step)
        #todo: process
        return True
    def __len__(self):
        return len(self.steps)
    def __iter__(self):
        return iter(self.steps)
    def __getitem__(self, key):
        return self.steps[key]
    def __setitem__(self, key, item):
        raise RuntimeError("Cannot set opstep through setitem overload.")
    def __delitem__(self, key):
        del self.steps[key]

    def remove_by_tag(self, tag):
        for index in range(len(self)-1, -1, -1):
            if self[index]["tag"]==tag:
                del self[index]

class OpStepTy:
    # New
    Malloc = 0
    Calloc = 1
    Memalign = 2
    # Alter
    Realloc = 3
    # Rm
    Free = 4
    # Misc
    Mallopt = 5
    ERROR = 6

class OpStep:
    def __init__(self, ty, name=None):
        global namegenerator
        self.ty = ty
        self._set_opty(ty)
        self.values = {}
        self.values["nokey"] = []
        if self.opty=="N" and (name is None or name=="GENerate"):
            name = next(namegenerator)
        self.values["name"] = name
        """
        N for New, D for delete, E for edit, M for misc
        """

    def _set_opty(self, ty):
        if ty==OpStepTy.Malloc or \
                ty==OpStepTy.Calloc or \
                ty==OpStepTy.Memalign:
            self.opty = "N"
        elif ty==OpStepTy.Realloc:
            self.opty = "E"
        elif ty==OpStepTy.Free:
            self.opty = "D"
        else:
            self.opty = "M"
        return

    def __getitem__(self, key):
        return self.values[key]
    def __setitem__(self, key, item):
        self.add(key, item)
    def __contains__(self, item):
        return item in self.values
    def __iter__(self):
        return iter(self.values)
    def __repr__(self):
        repr1 = "[{:s}: ".format(type(self).__name__, )
        repr2 = '; '.join(["{}={}".format(x,y) for (x,y) in self.values.items()])
        return repr1 + repr2+']'
    def add(self, key, value):
        if key is None:
            self.values["nokey"].append(value)
        else:
            self.values[key] = value
    def export(self):
        return "BASECLASS!"

class MallocStep(OpStep):
    def __init__(self, name=None, size=0):
        super().__init__(OpStepTy.Malloc, name)
        self.add("size",size)
    def export(self):
        if "tag" not in self or self["tag"] is None:
            return ''
        name = "X" if self["name"] is None else self["name"]
        size = 0 if "size" not in self else self["size"]
        tag = self["tag"]
        if 'bty' in self and self['bty']!='R':
            return '{:d}{:s}&{:s}({:s}:{:d})'.format(self.ty, tag, self['bty'], name, size)
        return '{:d}{:s}({:s}:{:d})'.format(self.ty, tag, name, size)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        (size, ret_addr, ret_ptr) = parse.parse("MALLOC({:d}) @ 0x{:x} = 0x{:x}", line)
        step = MallocStep(name, size)
        step.add("ret_addr",ret_addr)
        step.add("ret_ptr",ret_ptr)
        return step

class CallocStep(OpStep):
    def __init__(self, name=None, size=0):
        super().__init__(OpStepTy.Calloc)
        self.add("size",size)
    def export(self):
        if "tag" not in self or self["tag"] is None:
            return ''
        name = "X" if self["name"] is None else self["name"]
        size = 0 if "size" not in self else self["size"]
        nmemb = 0 if "nmemb" not in self else self["nmemb"]
        tag = self["tag"]
        if 'bty' in self and self['bty']!='R':
            return '{:d}{:s}&{:s}({:s}:{:d},{:d})'.format(self.ty, tag, self['bty'], name, nmemb, size)
        return '{:d}{:s}({:s}:{:d},{:d})'.format(self.ty, tag, name, nmemb, size)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        (nmemb, size, ret_addr, ret_ptr) = parse.parse("CALLOC({:d}, {:d}) @ 0x{:x} = 0x{:x}", line)
        step = CallocStep(name, size)
        step.add("ret_addr",ret_addr)
        step.add("ret_ptr",ret_ptr)
        step.add("nmemb",nmemb)
        return step

class MemalignStep(OpStep):
    def __init__(self, name=None, size=0, alignment=1):
        super().__init__(OpStepTy.Memalign, name)
        self.add("size",size)
        self.add("alignment",alignment)
    def export(self):
        if "tag" not in self or self["tag"] is None:
            return ''
        name = "X" if self["name"] is None else self["name"]
        size = 0 if "size" not in self else self["size"]
        alignment = 1 if "alignment" not in self else self["alignment"]
        tag = self["tag"]
        if 'bty' in self and self['bty']!='R':
            return '{:d}{:s}&{:s}({:s}:{:d},{:d})'.format(self.ty, tag, self['bty'], name, alignment, size)
        return '{:d}{:s}({:s}:{:d},{:d})'.format(self.ty, tag, name, alignment, size)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        (alignment, size, ret_addr, ret_ptr) = parse.parse("MEMALIGN({:d}, {:d}) @ 0x{:x} = 0x{:x}", line)
        step = MemalignStep(name, size, alignment)
        step.add("ret_addr", ret_addr)
        step.add("ret_ptr", ret_ptr)
        return step

class ReallocStep(OpStep):
    def __init__(self, name=None, ptr=0, newsize=0):
        super().__init__(OpStepTy.Realloc, name)
        self.add("ptr",ptr)
        self.add("size",newsize)
    def export(self):
        if "tag" not in self or self["tag"] is None:
            return ''
        name = "X" if self["name"] is None else self["name"]
        size = 0 if "size" not in self else self["size"]
        tag = self["tag"]
        if 'bty' in self and self['bty']!='R':
            return '{:d}{:s}&{:s}(:{:d})'.format(self.ty, tag, self['bty'], size)
        return '{:d}{:s}(:{:d})'.format(self.ty, tag, size)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        (oldptr, size, ret_addr, ret_ptr) = parse.parse("REALLOC(0x{:x}, {:d}) @ 0x{:x} = 0x{:x}", line)
        if oldptr==0:
            step = MallocStep(name, size)
        elif size==0:
            step = FreeStep(name, oldptr)
        else:
            step = ReallocStep(name, oldptr, size)
        step.add("ret_addr", ret_addr)
        step.add("ret_ptr", ret_ptr)
        return step

class FreeStep(OpStep):
    def __init__(self, name=None, ptr=0):
         super().__init__(OpStepTy.Free, name)
         self.add("ptr",ptr)
    def export(self):
        if "tag" not in self or self["tag"] is None:
            return ''
        name = "X" if self["name"] is None else self["name"]
        tag = self["tag"]
        return '{:d}{:s}()'.format(self.ty, tag)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        buff_ptr = parse.parse("FREE(0x{:x})", line).fixed[0]
        step = FreeStep(name, buff_ptr)
        return step

class MalloptStep(OpStep):
    def __init__(self, name=None, param=0, val=0):
         super().__init__(OpStepTy.Mallopt, name)
         self.add("param",param)
         self.add("value",val)
    def export(self):
        param = 0 if "param" not in self else self["param"]
        value = 0 if "value" not in self else self["value"]
        if "name" not in self or self["name"] is None:
            return '{:d}(:{:d},{:d})'.format(self.ty, param, value)
        return '{:d}({:s}:{:d},{:d})'.format(self.ty, self["name"], param, value)
    @classmethod
    def from_string(cls, line):
        (name, line) = extract_name(line)
        #"\tMALLOPT(%ld, %ld) = %ld",
        (param, val, retval) = parse.parse("MALLOPT({:d}, {:d}) {:d}", line)
        step = MalloptStep(name, param, val)
        step.add("retval", step)
        return step

def _name_generator(namelist):
    while True:
        pick_from = namelist.copy()
        while len(pick_from)>0:
            i = random.randint(0,len(pick_from)-1)
            name = pick_from[i]
            del pick_from[i]
            yield name


namere = None
def extract_name(line):
    global namere
    if namere is None:
        namere = re.compile("\(([a-zA-Z][a-zA-Z0-9]+)\)")
    res = namere.search(line)
    if res is None:
        return None, line
    return res.group(1), line[:res.start()] + line[res.end():]

## Namelist
namelist = [
"instal",
"anticipation",
"present",
"researcher",
"attic",
"contemporary",
"wage",
"democratic",
"discount",
"sailor",
"float",
"lean",
"project",
"jewel",
"value",
"harmful",
"step",
"landscape",
"leader",
"irony",
"snow",
"relief",
"wild",
"capital",
"pest",
"symptom",
"trace",
"revoke",
"honest",
"spray",
"soul",
"belt",
"rough",
"beer",
"stem",
"overcharge",
"assumption",
"stunning",
"packet",
"wind",
"marketing",
"name",
"suppress",
"organisation",
"council",
"road",
"pan",
"worm",
"matter",
"finished",
"route",
"prefer",
"lip",
"spit",
"knot",
"pasta",
"strain",
"celebration",
"stunning",
"pottery",
"queen",
"explicit",
"hen",
"acquaintance",
"imposter",
"favourite",
"tactic",
"painter",
"stay",
"surprise",
"rebellion",
"betray",
"remind",
"jump",
"pocket",
"provision",
"sunrise",
"bishop",
"ignorance",
"chew",
"jewel",
"so",
"drawing",
"hardware",
"reveal",
"investment",
"lobby",
"door",
"expectation",
"period",
"disk",
"privacy",
"flower",
"image",
"prosecute",
"degree",
"ground",
"notion",
"embark",
"recession"
]
namegenerator = _name_generator(namelist)
