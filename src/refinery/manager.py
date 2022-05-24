import logging
from operation import Operation, OpStepTy
import functools

class HeapRebuildManager:
    def __init__(self, **kwargs):
        self.reset()
        self._groups_dirty = True
        self._set_logger(**kwargs)
        ### All the other stuff 
        self._operation_mode = 'P' # ptmalloc
        self._attack_mode = 'OFA'
        self.heapsize = 1
        self.cleanup_init = False if "cleanup_init" not in kwargs else kwargs["cleanup_init"]

    @property
    def is_huge(self):
        def add(x,y): return x+y
        return 40 < functools.reduce(add,[len(op) for op in self.ops])

    def reset(self):
        self.runs = []
        self.libs = {}
        self.ops = []
        self._groups_dirty = True
        self._groups_cache = []
        self.tag_generator = _tag_generator()

    def postprocess(self):
        self.label()
        if self.cleanup_init:
            self.cleanup_init_experimental()

    def set_operation_mode(self, character):
        # First fit, ptmalloc, next fit, best fit, random fit,
        # dlmalloc, tcmalloc, jemalloc
        character = character[0].upper()
        if character in ['F', 'P', 'N', 'B', 'R', 'D', 'T', 'J']:
            self._operation_mode = character

    def set_attack_mode(self, name):
        if name.upper()=="OFA" or name.lower()=="overflowonallocation": # or name = overflow on allocation or ...
            self._attack_mode="OFA"
        elif name.upper()=="OVF" or name.lower()=="overflow":
            self._attack_mode="OVF"
        elif len(name)==3:
            self._attack_mode=name.upper()
        else:
            self.logger.warning("Unknown attack type: {}".format(name))
        self.logger.debug("\tNew attack type: {}".format(self._attack_mode))
        return

    def clean(self):
        for i in range(len(self.ops)-1, -1, -1):
            if len(self.ops[i]) < 1:
                del self.ops[i]

    def _set_logger(self, **kwargs):
        level = logging.INFO if "debug" not in kwargs else kwargs["debug"]
        self.logger = logging.getLogger("puzzleparser")
        self.logger.setLevel(level)
        ch = logging.StreamHandler()
        ch.setLevel(level)
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        ch.setFormatter(formatter)
        self.logger.addHandler(ch)
        if "logfile" in kwargs and kwargs["logfile"] is not None:
            ch = logging.FileHandler(kwargs["logfile"])
            ch.setLevel(logging.DEBUG)
            ch.setFormatter(formatter)
            self.logger.addHandler(ch)
        self.logger = logging.getLogger("puzzleparser.manager")


    def new_op(self, name="anon", isInit=False):
        if "init"==name:
            isInit=True
        op = Operation(self, name, isInit=isInit)
        self.ops.append(op)
        # We _could_ set a dirty bit here, and cache the groups property
        return op

    def new_exec(self, main_addr):
        self.runs.append((len(self.ops), main_addr))
        self.main_addr = main_addr

    def add_dynamic_library(self, name, base_addr):
        self.libs[base_addr] = name

    # Note: the returned list is sorted by design
    @property
    def groups(self):
        if self._groups_dirty:
            self._redetermine_groups()
        return self._groups_cache

    def _redetermine_groups(self):
        namedict = {}
        for i in range(len(self.ops)):
            op = self.ops[i]
            if op.name in namedict:
                namedict[op.name].append(i)
            else:
                namedict[op.name] = [i]
        self._groups_cache = list(namedict.values())
        self._groups_dirty = False

    def is_last(self, index):
        index += 1
        if index == len(self.ops):
            return True
        for x in runs:
            if x[0]==index:
                return True
        return False

    def cleanup_init_experimental(self):
        self.logger.warn("Warning! Cleanup init is still experimental and may change your solution!")
        cleanup_taglist = self._init_stable()
        self.logger.warn("Removing {} items.".format(len(cleanup_taglist)))
        self.logger.debug("To be deleted: " + ",".join(cleanup_taglist))
        for tag in cleanup_taglist:
            for op in self.ops:
                op.remove_by_tag(tag)
        self.clean()

    def export(self):
        self.clean()
        exportstr = "HPM2/"
        if self.is_huge:
            exportstr += "H"
        exportstr += self._operation_mode + self._attack_mode
        exportstr += str(self.heapsize) + "T"
        #exportstr += "POFA1T"
        opstrs = []
        for opi in range(len(self.ops)):
            if opi+1==len(self.ops) and (\
                    self.ops[opi].name.lower() == "end" or \
                    self.ops[opi].name.lower() == "bye"):
                self.logger.warn("Removing 'end' operation in export.")
                continue;
            opstrs.append("")
            if self.ops[opi].isInit:
                opstrs[-1] += '.'
            opstrs[-1] += self.ops[opi].name + ":"
            for stepi in range(len(self.ops[opi])):
                opstrs[-1] += self.ops[opi][stepi].export()
        exportstr += ".".join(opstrs)
        return exportstr

    def label(self):
        self.logger.debug("-------- Starting the labeling --------")
        for opi in range(len(self.ops)):
            for stepi in range(len(self.ops[opi])):
                opstep = self.ops[opi][stepi]
                self.logger.debug("Labeling ({},{}): {}".format(opi, stepi, opstep))
                if "tag" not in opstep:
                    try:
                        if opstep.opty == "N":
                            self._track_ptr(opi, stepi)
                        elif opstep.opty != "M":
                            # ignore on a free(0x0)
                            if not (opstep.opty=="D" and "ptr" in opstep and opstep["ptr"]==0):
                                self.logger.warning("Dangling opstep @ ({:d},{:d})! {}".format(opi, stepi,opstep))
                            opstep["tag"]='-'
                    except (AttributeError, RuntimeError) as e:
                        self.logger.error("Failed opstep: {:s} (reason: {:s})".format(opstep, str(e)))
        for opindex in range(len(self.ops)-1,-1,-1):
            for stepindex in range(len(self.ops[opindex])-1,-1,-1):
                if self.ops[opindex][stepindex]["tag"]=='-':
                    del self.ops[opindex][stepindex]

    def _track_ptr(self, opindex, stepindex):
        opstep = self.ops[opindex][stepindex]
        if "tag" in opstep:
            self.logger.debug("'{}' already has a tag ({})".format(opstep, opstep["tag"]))
            return
        tag = next(self.tag_generator)
        self.logger.info("Next tag: {}".format(tag))
        opstep.add("tag", tag)
        if "ret_ptr" not in opstep:
            raise AttributeError("opstep has no pointer to track.")
        if opstep.ty not in [OpStepTy.Malloc, OpStepTy.Calloc, OpStepTy.Memalign]:
            raise RuntimeError("Not a starting point for pointer tracking.")
        cnt = True
        while opindex >=0 and stepindex >=0:
            try:
                opstep = self.ops[opindex][stepindex]
                opstep.add("tag", tag)
                self.logger.info("Tagging next tracked ins of type {} @ ({:d},{:d})".format(opstep.ty, opindex,stepindex))
                (opindex, stepindex, cnt) = self._track_next(opindex, stepindex)
            except IndexError:
                break

    def _track_next(self, opindex, stepindex):
        opstep = self.ops[opindex][stepindex]
        if "ret_ptr" not in opstep:
            self.logger.info("Stopping current path: it's the end. (ty: {})".format(opstep.opty))
            return -1,-1,False
        ptr = opstep["ret_ptr"]
        stepindex += 1 # we can skip the starting step
        for i in range(opindex, len(self.ops)):
            for j in range(stepindex, len(self.ops[i])):
                opstep = self.ops[i][j]
                retptr = self._check_for_ptr(ptr, opstep)
                if retptr is not None:
                    return i,j, retptr>0
            # if it's not in current op, go to the next at the start
            stepindex = 0
        return -1,-1,False


    def _check_for_ptr(self, ptr, opstep):
        if opstep.opty == 'S':
            return None
        if 'ptr' not in opstep:
            return None
        if opstep['ptr']!=ptr:
            return None
        if 'ret_ptr' not in opstep:
            return -1
        return opstep['ret_ptr']


    def _init_stable(self):
        def is_eligible(opstep):
            return (opstep.opty=="N" and \
                    ("bty" not in opstep or opstep["bty"]=="R") and \
                    opstep.ty != OpStepTy.Memalign)
        stable = []
        for index in range(len(self.ops)):
            if not self.ops[index].isInit:
                continue
            prev = self.ops[index][0]
            opstep = self.ops[index][1]
            for j in range(2,len(self.ops[index])):
                next = self.ops[index][j]
                if is_eligible(opstep) and is_eligible(prev) and is_eligible(next):
                    stable.append(opstep["tag"])
                elif opstep.opty=="D" and opstep["tag"] in stable:
                    stable.remove(opstep["tag"])
                prev=opstep
                opstep=next
        for index in range(len(self.ops)):
            if self.ops[index].isInit:
                continue;
            for opstep in self.ops[index]:
                if opstep["tag"] in stable:
                    stable.remove(opstep["tag"])
        return stable

    def _init_stable_old(self):
        stable = []
        for index in range(len(self.ops)):
            if not self.ops[index].isInit:
                continue
            for opstep in self.ops[index]:
                if opstep.opty=="N" and ("bty" not in opstep or opstep["bty"]=="R") \
                        and opstep.ty != OpStepTy.Memalign:
                    stable.append(opstep["tag"])
                elif opstep.opty=="D" and opstep["tag"] in stable:
                    stable.remove(opstep["tag"])
        for index in range(len(self.ops)):
            if self.ops[index].isInit:
                continue;
            for opstep in self.ops[index]:
                if opstep["tag"] in stable:
                    stable.remove(opstep["tag"])
        return stable


def _tag_generator():
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

