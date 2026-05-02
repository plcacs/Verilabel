import csv
import sys
import os
from multiprocessing import Process
import re
import enum
import shutil
import networkx as nx
import matplotlib.pyplot as plt
from pycparser import c_ast, c_generator   # keep — SMTFormulaBuilder still needs these
from pycparser.c_ast import Return,UnaryOp, BinaryOp, If, NodeVisitor,FileAST, FuncDef,ID,TernaryOp

from collections import deque
Status = enum.Enum('Status',['FIXING','NOT_FIXING','SMT_TIMEOUT','NO_CONDITION_DIFF'])
sys.setrecursionlimit(100000)
generator = c_generator.CGenerator()
cfg = nx.DiGraph()
node_id = 0
before_file = ''
after_file = ''
file_name = ''
output_csv="smt_output.csv"
count=0

def extract_info_from_filename(filename):
    """
    Extracts the ID and CVE from a filename.
    Handles both:
    "187942_CVE-2016-0838-complete.c" -> ('187942', 'CVE-2016-0838')
    "177741_CVE-2011-4128.c"          -> ('177741', 'CVE-2011-4128')
    """
    # Looks for digits, an underscore, and then the specific CVE pattern
    match = re.search(r"(\d+)_(CVE-\d{4}-\d+)", filename)
    if match:
        return match.group(1), match.group(2)
    return None, None
def attach_parents(node, parent=None):
    """Recursively set .parent on all pycparser AST nodes."""
    if not isinstance(node, c_ast.Node):
        return
    # set parent for this node
    if parent is not None:
        # Avoid overwriting if already set; harmless either way
        try:
            node.parent
        except AttributeError:
            node.parent = parent
    # recurse into children
    for _, child in node.children():
        attach_parents(child, node)


class LabeledCondition:
    def __init__(self, node_label, condition_ast):
        self.node_label = node_label  # e.g. node id or label text
        self.condition_ast = condition_ast

def type_conversion(src_type, tgt_type, fml):
    if src_type == tgt_type:
        return fml
    elif tgt_type == 'Bool' and src_type != 'Bool':
        return f"({fml}) != 0"                                                                            
    elif src_type == 'Int' and tgt_type == 'Bool':
        new_fml = '{} != 0'.format(fml)
        return new_fml
    elif src_type == 'Bool' and tgt_type == 'Int':
        return f"If({fml}, 1, 0)"
    elif src_type == 'Int' and tgt_type == 'BitVec':
        new_fml = 'Int2BV({},32)'.format(fml)
        return new_fml
    elif src_type == 'BitVec' and tgt_type == 'Int':
        new_fml = 'BV2Int({})'.format(fml)
        return new_fml        
    elif src_type == 'BitVec' and tgt_type == 'Bool':
        new_fml = 'BV2Int({}) !=0'.format(fml)
        return new_fml               
    else:
        raise TypeError('Type conversion not supported from {} to {}'.format(src_type,tgt_type))

_CAST_TYPE_MODULUS = {
        'uint8_t': 256,         'FT_Byte': 256,    'FT_UInt8': 256,
        'uint16_t': 65536,      'FT_UShort': 65536, 'FT_UInt16': 65536,
        'uint32_t': 4294967296, 'FT_UInt32': 4294967296,
        'FT_UInt':  4294967296, 'FT_ULong': 4294967296,
        'unsigned': 4294967296, 'unsigned int': 4294967296,
        'size_t':   4294967296, 'uintptr_t': 4294967296,
        'int16_t':  65536,      'FT_Short': 65536,  'FT_Int16': 65536,
        'int32_t':  4294967296, 'FT_Int32': 4294967296,
        'FT_Int':   4294967296, 'FT_Long': 4294967296,
}

def _extract_cast_type(cast_ast):
    """Extract the type name string from a pycparser Cast node's to_type."""
    try:
        to_type = cast_ast.to_type
        if isinstance(to_type, c_ast.TypeDecl):
            inner = to_type.type
            if isinstance(inner, c_ast.IdentifierType):
                return ' '.join(inner.names)
    except Exception:
        pass
    return None
class SMTFormulaBuilder(NodeVisitor):
    arith_op = {'+','-', "*", '/'}
    rel_op = {'<','<=','>','>=','==','!='}
    logical_op = {'&&','||'}
    first_param_type = {'+':'Int', '-':'Int','*':'Int','/':'Int',
                        '<':'Int','<=':'Int','>':'Int','>=':'Int','==':'Int','!=':'Int',
                        '&&':'Bool','||':'Bool','>>':'Int','%':'Int'
                        ,'&':'BitVec','|':'BitVec','^':'BitVec','<<':'Int'}
    second_param_type = {'+':'Int', '-':'Int','*':'Int','/':'Int',
                        '<':'Int','<=':'Int','>':'Int','>=':'Int','==':'Int','!=':'Int',
                        '&&':'Bool','||':'Bool','>>':'Int','%':'Int',
                        '&':'BitVec','|':'BitVec','^':'BitVec','<<':'Int'}
    return_type = {'+':'Int', '-':'Int','*':'Int','/':'Int',
                        '<':'Bool','<=':'Bool','>':'Bool','>=':'Bool','==':'Bool','!=':'Bool',
                        '&&':'Bool','||':'Bool','!':'Bool','>>':'Int','%':'Int',
                        '&':'BitVec','|':'BitVec','^':'BitVec','<<':'Int'}
    format_template= {'+':'{}+{}', '-':'{}-{}','*':'{}*{}','/':'{}/{}', '>>':'{} >> {}',
                        '<':'{}<{}','<=':'{} <= {}','>':'{} > {}','>=':'{} >= {}','==':'{} == {}','!=':'{} != {}', '&&':'And({},{})','||':'Or({},{})','%':'{} % {}'
                        ,'&':'{} & {}','|':'{} | {}','^':'{} ^ {}','<<':'{} << {}'}

    unary_first_param_type = {'*':'Int','&':'Int','!':'Bool','++':'Int','p++':'Int','--':'Int','p--':'Int','~':'BitVec','sizeof':'Int', '-':'Int'}
    unary_return_type = {'*':'Int','&':'Int','!':'Bool','++':'Int','p++':'Int','--':'Int','p--':'Int','~':'BitVec','sizeof':'Int', '-':'Int'}
    unary_format_template= {'*':'{}','&':'{}','!':'Not({})','++':'{} + 1','p++':'{}','--':'{} - 1','p--':'{}','~':'~ {}','sizeof':'{}', '-': '(-{})'}
                        
    def __init__(self):
        self.generator = c_generator.CGenerator()
        self.funcs = set()
        self.vars = set()
        self.z3_decls = {}
        self._fallback_counter = 0  # to name unknown expressions deterministically
        self._unknown_cache = {}   # key -> base_var_name (canonical Int symbol)
        self._fallback_counter = 0
    @staticmethod        
    def format_fml(template,args):
        # print('------',*args)
        # print('------',template)
        return template.format(*args)
    
    def visit_BinaryOp(self,cond_ast):
        (ltype,lfml) = self.visit(cond_ast.left)
        (rtype,rfml) = self.visit(cond_ast.right)
        
        lfml = type_conversion(ltype,SMTFormulaBuilder.first_param_type[cond_ast.op],lfml)
        rfml = type_conversion(rtype,SMTFormulaBuilder.second_param_type[cond_ast.op],rfml)
        
        fml = SMTFormulaBuilder.format_fml(SMTFormulaBuilder.format_template[cond_ast.op],[lfml,rfml])
        return (SMTFormulaBuilder.return_type[cond_ast.op],fml)


    def visit_Constant(self, cond_ast):
        val = cond_ast.value
        if val in ["NULL", "((void *) 0)"]:
            return ('Int', "0")
        if val.startswith("'") and val.endswith("'"):
            return ('Int', str(ord(val[1])))  # char
        if val.startswith('"') and val.endswith('"'):
            name = f"str_{abs(hash(val))}"
            if name not in self.vars:
                self.vars.add(name)
                self.z3_decls[name] = f"{name} = Int('{name}')\n"
            return ('Int', name)
        return ('Int', val)
        
    def visit_ID(self,cond_ast):
        if cond_ast.name == 'INT_MAX':
            return ('Int','2147483647')
        if cond_ast.name not in self.vars:
            self.vars.add(cond_ast.name)
            self.z3_decls[cond_ast.name] = "{}=Int('{}')\n".format(cond_ast.name,cond_ast.name)
        return ('Int',cond_ast.name)

    def visit_StructRef(self,cond_ast):
        str_name = self.generator.visit(cond_ast)
        name = str_name.replace('.','_d_').replace('->','_p_')
        if name not in self.vars:
            self.vars.add(name)
            self.z3_decls[name] = "{}=Int('{}')\n".format(name,name)
            print('+++',self.z3_decls[name])
        return ('Int',name)
        
    def visit_UnaryOp(self,cond_ast):
        (ttype,fml) = self.visit(cond_ast.expr)
        
        fml = type_conversion(ttype,SMTFormulaBuilder.unary_first_param_type[cond_ast.op],fml)
        # print('*****',fml)
        
        fml = SMTFormulaBuilder.format_fml(SMTFormulaBuilder.unary_format_template[cond_ast.op],[fml])
        # print('*****',fml)
        return (SMTFormulaBuilder.unary_return_type[cond_ast.op],fml)
        
    def visit_FuncCall(self,cond_ast):
        if isinstance(cond_ast.name,ID):
            name = cond_ast.name.name
        else:
            #print('Name type does not know: ',type(cond_ast.name))
            #assert False
            name = f"fun_{abs(hash(self.generator.visit(cond_ast.name)))}"

        exprs = []
        if cond_ast.args is not None:
            # args can be an ExprList; normalize to a list safely
            if hasattr(cond_ast.args, "exprs") and isinstance(cond_ast.args.exprs, list):
                exprs = cond_ast.args.exprs
            else:
                exprs = [cond_ast.args]

        tfs = [self.visit(e) for e in exprs]
        arg_fmls = [type_conversion(t, 'Int', f) for (t, f) in tfs]
        num_args = len(arg_fmls)
        
        if name not in self.funcs:
            self.funcs.add(name)
            tempplate = "{} = Function('{}'" + ",IntSort()"*(num_args+1) +")\n"
            func_decl = tempplate.format(name,name)
            self.z3_decls[name] = func_decl
            
        fml = "{}({})".format(name,','.join(arg_fmls))
        
        return ('Int',fml)

    def visit_Cast(self, cond_ast):
        type_name = _extract_cast_type(cond_ast)
        modulus   = _CAST_TYPE_MODULUS.get(type_name) if type_name else None

        (inner_type, inner_fml) = self.visit(cond_ast.expr)

        if modulus is not None:
            inner_fml = type_conversion(inner_type, 'Int', inner_fml)
            return ('Int', f"({inner_fml}) % {modulus}")

        return (inner_type, inner_fml)


    #SMT formulas for ternary operations
    def visit_TernaryOp(self, cond_ast):
        print(">> SMT Ternary Detected:", self.generator.visit(cond_ast))
        (ctype, cfml) = self.visit(cond_ast.cond)
        (t1type, fml1) = self.visit(cond_ast.iftrue)
        (t2type, fml2) = self.visit(cond_ast.iffalse)

        if ctype != 'Bool':
            cfml = f"({cfml}) != 0"
            ctype = 'Bool'           
        # Pick a common target type using a priority: BitVec > Int > Bool
        type_priority = {'BitVec': 2, 'Int': 1, 'Bool': 0}
        target_type = t1type if type_priority.get(t1type, 0) >= type_priority.get(t2type, 0) else t2type

        # Coerce BOTH branches to the same target type
        fml1 = type_conversion(t1type, target_type, fml1)
        fml2 = type_conversion(t2type, target_type, fml2)

        return (target_type, f"If({cfml}, {fml1}, {fml2})")
    
    def visit_ArrayRef(self, cond_ast):
        base_type, base_fml = self.visit(cond_ast.name)
        idx_type, idx_fml = self.visit(cond_ast.subscript)
        name = f"{base_fml}_{idx_fml}"
        if name not in self.vars:
            self.vars.add(name)
            self.z3_decls[name] = f"{name} = Int('{name}')\n"
        return ('Int', name)
    
# ---- helpers ----------------------------------------------------------

    def _normalize_text(self, node):
        """Stable, whitespace-insensitive C text for a node, or None."""
        try:
            txt = self.generator.visit(node)
            # collapse whitespace to avoid formatting diffs
            return " ".join(txt.split())
        except Exception:
            return None

    def _unknown_key(self, node):
        """Build a stable key for unknown nodes."""
        txt = self._normalize_text(node)
        if txt:
            return ("txt", txt)
        # textual form failed; fall back to a structural-ish key
        # (type + children types) – not perfect, but deterministic
        kids = []
        try:
            for _, ch in node.children():
                kids.append(type(ch).__name__)
        except Exception:
            pass
        return ("shape", type(node).__name__, tuple(kids))
    
    def _expected_bool_context(self, node):
        """True iff node sits in a Boolean/predicate position (best effort)."""
        p = getattr(node, "parent", None)
        if p is None:
            return False
        # condition of control statements / ternary
        if isinstance(p, c_ast.If) and p.cond is node: return True
        if isinstance(p, c_ast.While) and p.cond is node: return True
        if isinstance(p, c_ast.DoWhile) and p.cond is node: return True
        if isinstance(p, c_ast.For) and p.cond is node: return True
        if isinstance(p, c_ast.TernaryOp) and p.cond is node: return True
        # logical / negation operators
        if isinstance(p, c_ast.UnaryOp) and p.op == '!': return True
        if isinstance(p, c_ast.BinaryOp) and p.op in {'&&', '||'}: return True
        return False

    
    def _fallback_symbol(self, node, want_bool=False):
        """
        Create or reuse a canonical Int symbol for an unknown expression.
        If a Bool is requested, we *wrap* the same Int symbol with '!= 0'.
        """
        key = self._unknown_key(node)
        if key in self._unknown_cache:
            base = self._unknown_cache[key]
        else:
            # mint a new Int symbol once
            self._fallback_counter += 1
            txt = self._normalize_text(node)
            # derive a readable/stable-ish name from text or counter
            if txt:
                base = f"unk_{abs(hash(txt))}"
            else:
                base = f"unk_{self._fallback_counter}"
            # avoid collisions
            i = 1
            name = base
            while name in self.vars:
                name = f"{base}_{i}"
                i += 1
            base = name
            self.vars.add(base)
            self.z3_decls[base] = f"{base}=Int('{base}')\n"
            self._unknown_cache[key] = base

        if want_bool:
            # return Bool view without creating a new symbol
            return ('Bool', f"({base}) != 0")
        else:
            return ('Int', base)

    # ---- FIXED: generic_visit uses the cache + boolean context -------------

    def generic_visit(self, cond_ast):
        """
        For any unhandled node type, reuse a canonical Int symbol keyed by text.
        If the node sits in a Boolean context, return the Bool-view (x != 0).
        """
        want_bool = self._expected_bool_context(cond_ast)
        return self._fallback_symbol(cond_ast, want_bool=want_bool)    

def build_cfg(funcdef):

    cfg = nx.DiGraph()
    node_id = [0]

    # Maps for resolving jumps after initial pass
    label_to_node = {}
    unresolved_gotos = []      # tuples (goto_node, target_label)
    unresolved_breaks = []     # break nodes to connect to exit
    unresolved_continues = []  # continue nodes to connect to exit (or loop head)
    unresolved_returns = []    # return nodes to connect to exit
    condition_stack = []
    def new_node(label):
        nid = f"{node_id[0]}: {label}"
        node_id[0] += 1
        cfg.add_node(nid, label=label)
        return nid


    #Returns (entries, exits) for this block.
    def process_block(block, incoming):
        entries = []
        exits = incoming[:]
        stmts = block.block_items if isinstance(block, c_ast.Compound) else [block]
        if isinstance(block, list):                         # FIX
            stmts = block
        elif isinstance(block, c_ast.Compound):
            stmts = block.block_items or []
        else:
            stmts = [block]

        for i, stmt in enumerate(stmts or []):
            stmt_entries, stmt_exits = process_statement(stmt, exits)
            if i == 0:
                entries = stmt_entries[:]
            exits = stmt_exits

        if not stmts:
            return incoming, incoming
        return entries, exits

    #current/incoming is actually the graph that is creating, block/stmt is function body or body item
    def process_statement(stmt, incoming):
        if isinstance(stmt, list):                          # FIX
            return process_block(stmt, incoming)
        # If statement
        if isinstance(stmt, c_ast.If):
            cond_ast  = stmt.cond
            cond_text = generator.visit(stmt.cond).replace("\n", " ") if stmt.cond else "UnknownCondition"
            cond_node = new_node("If " + cond_text)
            cfg.nodes[cond_node]['ast_node'] = stmt.cond
            for n in incoming:
                cfg.add_edge(n, cond_node)
            condition_stack.append(cond_ast) # <--- pushing condition
            # True branch
            then_entries, then_exits = process_block(stmt.iftrue, [cond_node])
            # Immediately annotate each first‐level child of the true‐branch
            if then_entries:
                cfg.add_edge(cond_node, then_entries[0], label="T")
            elif then_exits:
                cfg.add_edge(cond_node, then_exits[0], label="T")

            # False branch
            if stmt.iffalse:
                else_entries, else_exits = process_block(stmt.iffalse, [cond_node])
                if else_entries:
                    cfg.add_edge(cond_node, else_entries[0], label="F")
            else:
                else_entries, else_exits = [cond_node], [cond_node]

            condition_stack.pop()  # <--- popping condition
            # Merge: entry is test node; exit is union of branch exits
            return [cond_node], then_exits + else_exits

    
        elif isinstance(stmt, c_ast.Label):
            label_node = new_node(f"Label {stmt.name}")
            label_to_node[stmt.name] = label_node
            for n in incoming:
                cfg.add_edge(n, label_node)
            _, exits = process_block(stmt.stmt, [label_node])
            return [label_node], exits


        elif isinstance(stmt, c_ast.Goto):
            goto_node = new_node(f"Goto {stmt.name}")
            for n in incoming:
                cfg.add_edge(n, goto_node, label="goto")
                cfg.nodes[goto_node]['parent_condition'] = condition_stack[-1] if condition_stack else None
            unresolved_gotos.append((goto_node, stmt.name))
            return [goto_node], []


        elif isinstance(stmt, c_ast.Return):
            ret_node = new_node("Return")
            cfg.nodes[ret_node]['ast_node'] = stmt
    # Attach the parent condition (if any)
            cfg.nodes[ret_node]['parent_condition'] = condition_stack[-1] if condition_stack else None

            for n in incoming:
                cfg.add_edge(n, ret_node, label="return")

            if isinstance(stmt.expr, c_ast.TernaryOp):
                ternary_cond_node = new_node("Ternary " + generator.visit(stmt.expr.cond))
                cfg.nodes[ternary_cond_node]['ast_node'] = stmt.expr.cond
                cfg.add_edge(ret_node, ternary_cond_node)
                cfg.nodes[ternary_cond_node]['parent_condition'] = condition_stack[-1] if condition_stack else None
                unresolved_returns.append(ternary_cond_node)
            else:
                unresolved_returns.append(ret_node)
            return [ret_node], []
 
        elif isinstance(stmt, c_ast.Break):
            brk_node = new_node("Break")
            for n in incoming:
                cfg.add_edge(n, brk_node, label="break")
            unresolved_breaks.append(brk_node)
            return [brk_node], []


        elif isinstance(stmt, c_ast.Continue):
            cont_node = new_node("Continue")
            for n in incoming:
                cfg.add_edge(n, cont_node, label="continue")
            unresolved_continues.append(cont_node)
            return [cont_node], []
        
        elif isinstance(stmt, c_ast.Switch):
            cond_ast = stmt.cond
            cond_text = generator.visit(stmt.cond).replace("\n", " ") if stmt.cond else "UnknownSwitchCondition"
            switch_node = new_node("Switch " + cond_text)
            cfg.nodes[switch_node]['ast_node'] = stmt.cond

            for n in incoming:
                cfg.add_edge(n, switch_node)

            condition_stack.append(cond_ast)  # push condition

            case_exits = []
            case_nodes = []       # (case_node, exits) for fall-through
            pending_cases = []    # empty case/default nodes waiting for a body

            items = stmt.stmt.block_items or []

            for idx, item in enumerate(items):
                if isinstance(item, c_ast.Case):
                    label_text = f"case {generator.visit(item.expr)}"
                    label_node = new_node(f"Case {generator.visit(item.expr)}")
                    cfg.add_edge(switch_node, label_node, label=label_text)

                    # Link any previous empty case labels to this case
                    for prev_case_node in pending_cases:
                        cfg.add_edge(prev_case_node, label_node)
                    pending_cases = []

                    if hasattr(item, 'stmts') and item.stmts:
                        body_entries, body_exits = process_block(item.stmts, [label_node])
                        case_nodes.append((label_node, body_exits))
                    else:
                        pending_cases.append(label_node)

                elif isinstance(item, c_ast.Default):
                    label_node = new_node("Default")
                    cfg.add_edge(switch_node, label_node, label="default")

                    for prev_case_node in pending_cases:
                        cfg.add_edge(prev_case_node, label_node)
                    pending_cases = []

                    if hasattr(item, 'stmts') and item.stmts:
                        body_entries, body_exits = process_block(item.stmts, [label_node])
                        case_nodes.append((label_node, body_exits))
                    else:
                        pending_cases.append(label_node)

                else:
                    # Rare: statements directly under switch
                    block_entries, block_exits = process_block(item, [switch_node])
                    case_exits.extend(block_exits)

            # Link any leftover empty cases to exit
            case_exits.extend(pending_cases)

            # Handle fall-through edges
            for i in range(len(case_nodes) - 1):
                current_case, current_exits = case_nodes[i]
                next_case, _ = case_nodes[i + 1]

                for exit_node in current_exits:
                    label = cfg.nodes[exit_node]['label'].lower()
                    if not any(keyword in label for keyword in ["break", "return", "goto"]):
                        cfg.add_edge(exit_node, next_case, label="fall-through")

    # Collect exits from all cases
            for _, exits in case_nodes:
                case_exits.extend(exits)

            condition_stack.pop()  # pop condition

            return [switch_node], case_exits


        # while
        elif isinstance(stmt, c_ast.While):
            cond_ast = stmt.cond
            cond_text = generator.visit(cond_ast).replace("\n", " ") if cond_ast else "UnknownWhileCond"
            cond_node = new_node("While " + cond_text)
            cfg.nodes[cond_node]['ast_node'] = cond_ast

            for n in incoming:
                cfg.add_edge(n, cond_node)
            
            condition_stack.append(cond_ast) # <--- pushing condition
            body_entries, body_exits = process_block(stmt.stmt, [cond_node])
            for cont_node in unresolved_continues:
                cfg.add_edge(cont_node, cond_node, label="continue")
            unresolved_continues.clear()
            for exit_node in body_exits:
                cfg.add_edge(exit_node, cond_node, label="loop")
            
            for brk_node in unresolved_breaks:
                cfg.add_edge(brk_node, cond_node)  # Or link to loop exit
            unresolved_breaks.clear()
            
            condition_stack.pop()  # <--- popping condition
            return [cond_node], [cond_node]  # can exit at the loop condition

        elif isinstance(stmt, c_ast.DoWhile):
            body_node = new_node("DoWhile Body")
            cond_ast = stmt.cond
            cond_text = generator.visit(cond_ast).replace("\n", " ") if cond_ast else "UnknownDoWhileCond"
            cond_node = new_node("DoWhile " + cond_text)
            cfg.nodes[cond_node]['ast_node'] = cond_ast

            for n in incoming:
                cfg.add_edge(n, body_node)
            condition_stack.append(cond_ast) # <--- pushing condition
            body_entries, body_exits = process_block(stmt.stmt, [body_node])
            
            for cont_node in unresolved_continues:
                cfg.add_edge(cont_node, cond_node, label="continue")
            unresolved_continues.clear()
            
            for exit_node in body_exits:
                cfg.add_edge(exit_node, cond_node)
            
            for brk_node in unresolved_breaks:
                cfg.add_edge(brk_node, cond_node)
            unresolved_breaks.clear()
            cfg.add_edge(cond_node, body_node, label="loop")
            condition_stack.pop() # <--- popping condition
            return [body_node], [cond_node]
        
        elif isinstance(stmt, c_ast.For):
            init = generator.visit(stmt.init).replace("\n", " ") if stmt.init else ""
            cond_ast = stmt.cond
            cond_text = generator.visit(cond_ast).replace("\n", " ") if cond_ast else "True"
            next_text = generator.visit(stmt.next).replace("\n", " ") if stmt.next else ""

            cond_node = new_node("ForCond " + cond_text)
            cfg.nodes[cond_node]['ast_node'] = cond_ast

            init_node = new_node("ForInit " + init) if stmt.init else cond_node
            if stmt.init:
                for n in incoming:
                    cfg.add_edge(n, init_node)
                cfg.add_edge(init_node, cond_node)
            else:
                for n in incoming:
                    cfg.add_edge(n, cond_node)
            condition_stack.append(cond_ast) # <--- pushing condition
            body_entries, body_exits = process_block(stmt.stmt, [cond_node])
            if stmt.next:
                next_node = new_node("ForNext " + next_text)
                for exit_node in body_exits:
                    cfg.add_edge(exit_node, next_node)
                cfg.add_edge(next_node, cond_node)
            else:
                for exit_node in body_exits:
                    cfg.add_edge(exit_node, cond_node)
            for cont_node in unresolved_continues:
                cfg.add_edge(cont_node, cond_node, label="continue")
            unresolved_continues.clear()
            for brk_node in unresolved_breaks:
                cfg.add_edge(brk_node, cond_node)
            unresolved_breaks.clear()
            condition_stack.pop() # <--- popping condition
            return [init_node if stmt.init else cond_node], [cond_node]
        #Ternary operator detection
        elif isinstance(stmt, c_ast.Decl) and isinstance(stmt.init, c_ast.TernaryOp):
            cond_ast = stmt.init.cond
            cond_node = new_node("Ternary " + generator.visit(cond_ast))
            cfg.nodes[cond_node]['ast_node'] = cond_ast
            cfg.nodes[cond_node]['parent_condition'] = condition_stack[-1] if condition_stack else None
            for n in incoming:
                cfg.add_edge(n, cond_node)
            return [cond_node], [cond_node]
        elif isinstance(stmt, c_ast.Assignment) and isinstance(stmt.rvalue, c_ast.TernaryOp):
            cond_ast = stmt.rvalue.cond
            cond_node = new_node("Ternary " + generator.visit(cond_ast))
            cfg.nodes[cond_node]['ast_node'] = cond_ast
            cfg.nodes[cond_node]['parent_condition'] = condition_stack[-1] if condition_stack else None
            for n in incoming:
                cfg.add_edge(n, cond_node)
            return [cond_node], [cond_node]

        else:
            text = generator.visit(stmt).strip().replace("\n", " ")
            stmt_node = new_node(text)
            cfg.nodes[stmt_node]['ast_node'] = stmt
            for n in incoming:
                cfg.add_edge(n, stmt_node)
            return [stmt_node], [stmt_node]

    entry = new_node("Start")
    exit_node = new_node("End")


    _, final_nodes = process_block(funcdef.body, [entry])


    for goto_node, label in unresolved_gotos:
        if label in label_to_node:
            cfg.add_edge(goto_node, label_to_node[label], label="goto")
        else:
            print(f"Warning: unresolved goto to {label}")


    for ret in unresolved_returns:
        cfg.add_edge(ret, exit_node)

    for n in final_nodes:
        if n not in unresolved_returns:
            cfg.add_edge(n, exit_node)
    
    return entry, exit_node, cfg

def conjunct_conditions(cond_list):
    if not cond_list:
        return None, []
    conj = None
    labels = []
    for cond in cond_list:
        conj = cond.condition_ast if conj is None else BinaryOp('&&', conj, cond.condition_ast)
        labels.append(cond.node_label)
    return conj, labels

def disjoin_conditions(conjuncts_with_labels):
    if not conjuncts_with_labels:
        return None, []
    disj, labels = conjuncts_with_labels[0]
    all_labels = list(labels)
    for ast, lbls in conjuncts_with_labels[1:]:
        disj = BinaryOp('||', disj, ast)
        all_labels.extend(lbls)
    return disj, all_labels

def is_early_return_node(node, cfg):
    from pycparser.c_ast import Return
    label = cfg.nodes[node].get('label', '').lower()
    ast = cfg.nodes[node].get('ast_node')
    if "return" in label or "goto" in label:
        return True
    if isinstance(ast, Return):
        return True
    return False

from pycparser.c_ast import FuncCall, ID

def is_function_name_with_return(node, cfg):
    ast = cfg.nodes[node].get('ast_node')

    # Check if this node is a function call
    if isinstance(ast, FuncCall):
        # Extract function name
        if isinstance(ast.name, ID):
            func_name = ast.name.name.lower()
            if "return" in func_name:
                return True
    return False

def remove_duplicate_conditions(conds):
    """Remove duplicate conditions based on their AST string representation."""
    seen = set()
    unique = []
    for cond in conds:
        key = generator.visit(cond.condition_ast)
        if key not in seen:
            seen.add(key)
            unique.append(cond)
    return unique


def branch_all_early_returns(cfg, start_node, visited=None):
    """Return True if *all* paths from start_node hit an early return (or goto)."""
    if visited is None:
        visited = set()
    if start_node in visited:
        return True  # prevent loops being counted as false
    visited.add(start_node)

    if is_early_return_node(start_node, cfg) or is_function_name_with_return(start_node, cfg):
        return True

    succs = list(cfg.successors(start_node))
    if not succs:
        return False  # reached end without early return
    return all(branch_all_early_returns(cfg, s, visited) for s in succs)



from collections import deque
from pycparser.c_ast import UnaryOp, Return

def annotate_cfg_with_conditions(cfg, entry_node):
    MAX_PATH_DEPTH = 16

    # Initialize storage
    for node in cfg.nodes:
        cfg.nodes[node]['path_conditions_sets'] = []

    queue = deque()
    queue.append((entry_node, []))  # (current_node, path_conditions)
    visited = {}

    while queue:
        current, conditions = queue.popleft()

        # Avoid re-processing identical condition sets for a node
        cond_signature = '|'.join(sorted(generator.visit(c.condition_ast) for c in conditions))
        if current in visited and cond_signature in visited[current]:
            continue
        visited.setdefault(current, set()).add(cond_signature)

        # Store the path condition at this node
        cfg.nodes[current]['path_conditions_sets'].append(conditions)

        cond_ast = cfg.nodes[current].get('ast_node')
        label_text = cfg.nodes[current].get('label', '').lower()

        # Check if this is a conditional node
        is_conditional_node = any(k in label_text for k in ['if', 'for', 'while', 'do', 'switch', 'ternary'])

        # Detect implicit else after early return in true branch
        implicit_false_nodes = set()
        if is_conditional_node and cond_ast is not None:
            true_succs = [s for s in cfg.successors(current) if cfg.edges[current, s].get('label') == 'T']
            false_succs = [s for s in cfg.successors(current) if cfg.edges[current, s].get('label') == 'F']

            if not false_succs and true_succs:  # no explicit else
                all_return = all(branch_all_early_returns(cfg, t) for t in true_succs)
                if all_return:
                    implicit_false_nodes = {s for s in cfg.successors(current)
                                             if cfg.edges[current, s].get('label', '') != 'T'}

        # Explore successors
        for succ in cfg.successors(current):
            edge_label = cfg.edges[current, succ].get('label', '')
            new_conditions = list(conditions)

            if is_conditional_node and cond_ast is not None:
                if edge_label == "T":
                    new_conditions.append(LabeledCondition(cfg.nodes[current]['label'], cond_ast))
                elif edge_label == "F":
                    new_conditions.append(LabeledCondition(cfg.nodes[current]['label'], UnaryOp('!', cond_ast)))
                elif succ in implicit_false_nodes:
                    # Implicit false branch gets negated condition
                    new_conditions.append(LabeledCondition(cfg.nodes[current]['label'], UnaryOp('!', cond_ast)))
                else:
                    # default: treat like true branch if unlabeled
                    new_conditions.append(LabeledCondition(cfg.nodes[current]['label'], cond_ast))

            if len(new_conditions) <= MAX_PATH_DEPTH:
                queue.append((succ, new_conditions))

    # Combine all path conditions for each node
    for n in cfg.nodes:
        condition_sets = cfg.nodes[n].get('path_conditions_sets', [])
        conjuncts = []
        for conds in condition_sets:
            ast, labels = conjunct_conditions(remove_duplicate_conditions(conds))
            if ast:
                conjuncts.append((ast, labels))

        disj, disj_labels = disjoin_conditions(conjuncts)
        cfg.nodes[n]['conj_condition'] = disj
        cfg.nodes[n]['conj_condition_labels'] = disj_labels

    return cfg
def construct_header(builder):
    z3_code = "from z3 import *\ns = Solver()\n"
    for decl in builder.z3_decls.values():
        z3_code += decl
    return z3_code


def build_smt_formula_from_cfg(cond_b, cond_a):
    builder = SMTFormulaBuilder()

    # Build formula for before condition
    # Before
    bfml = None
    try:
        if cond_b is None:
            bfml = "True"
        else:
            (btype, bfml) = builder.visit(cond_b)
            bfml = type_conversion(btype, 'Bool', bfml)
    except Exception:
        # Last-resort fallback
        pass

    # After
    afml = None
    try:
        if cond_a is None:
            afml = "True"
        else:
            (atype, afml) = builder.visit(cond_a)
            afml = type_conversion(atype, 'Bool', afml)
    except Exception:
        pass
    # Signal failure back to caller instead of masking it
    if bfml is None or afml is None:
        return None, ('before' if bfml is None else 'after')
    # Build Z3 formula
    vars_list = list(builder.vars)
    if vars_list:
        vars_str = ','.join(vars_list)
        a2b_fml = f"s.add(ForAll([{vars_str}], Implies({afml}, {bfml})))\n"
        b2a_fml = f"s.add(Exists([{vars_str}], Implies({bfml}, Not({afml}))))\n"
    else:
        a2b_fml = f"s.add(Implies({afml}, {bfml}))\n"
        b2a_fml = f"s.add(Not(Implies({bfml}, {afml})))\n"

    # Combine formulas
    ender = 'solving_status=Z3_L_TRUE==s.check().r\n'
    z3_code = construct_header(builder) + a2b_fml + b2a_fml+ ender
    return z3_code, None


def compare_cfg_conditions(before_file,after_file,cfg_before, cfg_after, file_name, count):
    prefix_before=''
    prefix_after=''
    label_to_node_before = {data['label']: n for n, data in cfg_before.nodes(data=True)}
    label_to_node_after = {data['label']: n for n, data in cfg_after.nodes(data=True)}

    all_labels = set(label_to_node_before.keys()) | set(label_to_node_after.keys())
    diffs = []
    temp = 0

    with open("report_diff.txt", 'a') as report, open("limitation.log", 'a') as limitation, open("NotFixing.log",'a') as NotFixing, open('non_conditionalfix.log', 'a') as NonConditionalFixing:
        for label in sorted(all_labels):
            node_b = label_to_node_before.get(label)
            node_a = label_to_node_after.get(label)

            label_b = cfg_before.nodes[node_b]['label'] if node_b else None
            label_a = cfg_after.nodes[node_a]['label'] if node_a else None
            """if node_b and node_a:
                    label_b = cfg_before.nodes[node_b]['label'] if node_b else None
                    label_a = cfg_after.nodes[node_a]['label'] if node_a else None
            """
            cond_b = cfg_before.nodes[node_b]['conj_condition'] if node_b else None
            cond_a = cfg_after.nodes[node_a]['conj_condition'] if node_a else None

            cond_b_str = generator.visit(cond_b) if cond_b else "None"
            cond_a_str = generator.visit(cond_a) if cond_a else "None"

            
            labels_b = cfg_before.nodes[node_b].get('conj_condition_labels', []) if node_b else []
            labels_a = cfg_after.nodes[node_a].get('conj_condition_labels', []) if node_a else []
            origin_labels = set(labels_b + labels_a)
            # Check if this node represents an 'If'
            #is_if = any("if" in lbl.lower() for lbl in origin_labels)# if a node is originated from an if
            is_conditional = any(any(kw in lbl.lower() for kw in ["if", "while", "for", "do", "switch","ternary"]) for lbl in origin_labels)

            if label_b != label_a and is_conditional is False and cond_b is not None:
            # NEW: try non-conditional fix heuristics, conservatively
                
                NonConditionalFixing.write(f"'{count}'--------'{file_name}'--------\n")
                NonConditionalFixing.write(f"Non conditional fix detected at '{label}':\n")
                NonConditionalFixing.write(f"  Before Node: {label_b}\n")
                NonConditionalFixing.write(f"  After Node : {label_a}\n")
                    #if reason:
                        #NonConditionalFixing.write(f"  Reason     : {reason}\n")
                        #NonConditionalFixing.write("\n")
                #else:
                    #limitation.write(f"'{count}'--------'{file_name}'--------\n")
                    #limitation.write(f"Node change detected at '{label}':\n")
                    #limitation.write(f"  Before Node: {label_b}\n")
                    #limitation.write(f"  After Node : {label_a}\n\n")
                print(f"Node change detected at '{label}': {label_b} -> {label_a}")
                continue  # skip Z3 generation
            
            if cond_b_str != cond_a_str:
                
                # Prefer Z3 if conditional difference in 'if'
                if is_conditional:
                    func_changed, why = should_skip_smt_due_to_func_change(cond_b, cond_a)
                    if func_changed and cond_b is not None:
                        # Treat this as an analyzable non-conditional change (your existing bucket),
                        # or mark as "needs dataflow" and avoid SMT.
                        
                        NotFixing.write(f"'{count}'--------'{file_name}'--------\n")
                        NotFixing.write(f"Function-call change inside condition at node '{label}': {why}\n")
                        NotFixing.write(f"Notfixing change at node '{label}':\n")
                        NotFixing.write(f"  Before: {cond_b_str}\n")
                        NotFixing.write(f"  After : {cond_a_str}\n\n")
                        prefix_before='./Not_fixing/before/'
                        prefix_after='./Not_fixing/after/'
                        # continue (skip SMT for this pair)
                        continue
                    
                    
                    smt_code, failure_side  = build_smt_formula_from_cfg(cond_b, cond_a)
                    if smt_code is None:
                        print(f"Skipping SMT for '{label}': translation failed on {failure_side} side")
                        continue
                    with open("smt_formula_output.py", "a") as f:
                        f.write(f"# --- {file_name} Node: {label} ---\n")
                        f.write(smt_code + "\n\n")
                    
                    print(smt_code)
                    file_id, cve = extract_info_from_filename(file_name)
                    data_row = [file_id, cve, label, smt_code]
                    file_exists = os.path.isfile(output_csv)
                    needs_header = (not file_exists) or os.path.getsize(output_csv) == 0  # Empty file case
                    with open(output_csv, 'a', newline='', encoding='utf-8') as csvfile:
                        writer = csv.writer(csvfile)
                        if needs_header:
                            writer.writerow(['id', 'cve', 'node', 'smtcode'])
                        writer.writerow(data_row)
                    temp += 1
                    diffs.append((label, cond_b_str, cond_a_str))
                    print(f"Path condition difference at 'if' node '{label}':\n")
                    print(f"  Before: {cond_b_str}\n")
                    print(f"  After : {cond_a_str}\n")
                    with open("report_diff.txt", 'a') as report:
                        report.write(f"'{count}'---------'{file_name}'---------\n")
                        report.write(f"Path condition difference at node '{label}':\n")
                        report.write(f"  Before: {cond_b_str}\n")
                        report.write(f"  After : {cond_a_str}\n\n")
                    prefix_before='./Fixing/before/'
                    prefix_after='./Fixing/after/'
                    
                
                else:
                    # Not an if, log as limitation
                    NotFixing.write(f"'{count}'--------'{file_name}'--------\n")
                    NotFixing.write(f"Non-if or SMT condition change at node '{label}':\n")
                    NotFixing.write(f"  Before: {cond_b_str}\n")
                    NotFixing.write(f"  After : {cond_a_str}\n\n")
                    print(f"Inside not_fixing '{file_name}'node:'{label}'\n")
                    print(f"  Before: {cond_b_str}\n")
                    print(f"  After : {cond_a_str}\n\n")
                    prefix_before='./Not_fixing/before/'
                    prefix_after='./Not_fixing/after/'
                

    if len(diffs) > 0:
        
        prefix_before='./Fixing/before/'
        prefix_after='./Fixing/after/'
        
    else:
        print(f"'{count}'---------'{file_name}'---------\n")
        print(f"No conditional difference found at '{file_name}'\n")
        prefix_before='./Not_fixing/before/'
        prefix_after='./Not_fixing/after/'
            
    # Fixed — only copy if a destination was actually set
    if prefix_before and prefix_after:
        shutil.copy2(before_file, prefix_before + file_name)
        shutil.copy2(after_file,  prefix_after  + file_name)
    return diffs

def _find_funcdef(nodes):
    """Depth-first search for the first FuncDef anywhere in ast.ext."""
    for node in nodes:
        if isinstance(node, c_ast.FuncDef):
            return node
        # Compound bodies can contain nested FuncDefs in GNU C
        if isinstance(node, c_ast.Compound) and node.block_items:
            result = _find_funcdef(node.block_items)
            if result:
                return result
    return None
def generate_cfg_from_file(c_file_path,file_name,count, output_dot_path="cfg.dot", output_png_path="cfg.png"):

    # Parse the C file
    try:
        # Parse the C file
        ast = parse_c_file(c_file_path)
    except Exception as e:
        raise Exception(f"Failed to parse C file '{c_file_path}': {e}")
    #ast.show()
    # Extract the function definition
    attach_parents(ast)
    funcdef = _find_funcdef(ast.ext)
    if not funcdef:
        raise Exception("No function definition found in file.")

    # Build CFG and annotate with path conditions
    entry_node, exit_node, cfg = build_cfg(funcdef)

    annotate_cfg_with_conditions(cfg, entry_node)
    
    print("Cumulative path conditions per node:")
    temp=0
    with open("pathcond.txt", 'a') as cond:
        cond.write(f"'{count}'-----------'{file_name}'\n")
        for n, data in cfg.nodes(data=True):
            conj = data.get('conj_condition')
            labels = data.get('conj_condition_labels', [])
            if conj is not None:
                temp+=1
                text = generator.visit(conj)
                #print(f"Node {n} [{data['label']}]: {text} (Labels: {labels})\n")
                cond.write(f"Node {n} [{data['label']}]: {text} (Labels: {labels})\n")
        if temp==0:
            print(f"No path condition found at '{file_name}'\n")
            cond.write(f"No path condition found at '{file_name}'\n")
    # Optionally export CFG to DOT and PNG


    return entry_node, exit_node, cfg

def process_file_pair(before_file, after_file, file_name, count):
    try:
        entry_b, exit_b, cfg_b = generate_cfg_from_file(before_file, file_name, count)
        entry_a, exit_a, cfg_a = generate_cfg_from_file(after_file, file_name, count)
        compare_cfg_conditions(before_file,after_file,cfg_b, cfg_a, file_name, count)
        del cfg_b
        del cfg_a
        import gc
        gc.collect()
    except Exception as inst:
        with open('exceptions.log','a') as exception:
            exception.write(str(count)+'\n')
            exception.write(file_name+'\n')
            exception.write(str(type(inst))+'\n')
            exception.write(str(inst.args)+'\n')
            exception.write(str(inst)+'\n')



def main():
    #open('smt_formula_output.py', 'w').close()
    #open('limitation.log', 'w').close()
    #open('non_conditionalfix.log', 'w').close()
    open('report_diff.txt', 'w').close()
    #open('pathcond.txt', 'w').close()
    #open('exceptions.log', 'w').close()
    #open('NotFixing.log','w').close()
    global before_file, after_file, file_name,count
    before_dir = './before'
    after_dir = './after'
    for entry in os.scandir(before_dir):
        
        print('========= ', entry.name, ' ==============')
        before_file = os.path.join(before_dir,entry.name)
        after_file = os.path.join(after_dir,entry.name)
        file_name = entry.name
        count+=1
        if not os.path.exists(after_file):
            print(f"Skipping {file_name} — after version not found.")
            continue
        print(file_name,file=sys.stderr)
        p = Process(target=process_file_pair, args=(before_file, after_file, file_name, count))
        p.start()
        p.join(timeout=30)  # set timeout seconds as you prefer
#for debug purpose turning off
# ---------- SMT suitability filter ----------

_gen = c_generator.CGenerator()

def _expr_to_text(n):
    try:
        return _gen.visit(n)
    except Exception:
        return repr(type(n))

def _collect_func_calls_in_cond(ast):
    """
    Return a list of call 'signatures' found inside a condition AST.
    Each signature is a tuple: (callee_text, arity, args_text_tuple)
    Callee text falls back to stringified expression for non-ID callees.
    """
    calls = []

    class V(c_ast.NodeVisitor):
        def visit_FuncCall(self, node: c_ast.FuncCall):
            # callee can be ID or any expression (e.g., (*fp)(), obj->m())
            if isinstance(node.name, c_ast.ID):
                callee = node.name.name
            else:
                # textualize non-ID to keep a stable comparison
                callee = _expr_to_text(node.name)  # e.g., "(*fp)" or "obj->fn"

            # normalize args to a list
            exprs = []
            if node.args is None:
                exprs = []
            elif hasattr(node.args, "exprs") and isinstance(node.args.exprs, list):
                exprs = node.args.exprs
            else:
                exprs = [node.args]

            arity = len(exprs)
            args_text = tuple(_expr_to_text(e) for e in exprs)

            calls.append((callee, arity, args_text))

            # keep walking in case of nested calls in args
            self.generic_visit(node)

        # keep default generic visiting for all other nodes

    if ast is not None:
        V().visit(ast)
    return calls

def functions_changed_between(b_ast, a_ast):
    """
    Return (changed: bool, reason: str, details: dict) comparing call sets between
    before/after condition ASTs.

    We consider it CHANGED if:
      - A callee name appears only on one side, OR
      - The same callee appears but arity differs, OR
      - The same callee+arity appears but argument texts differ (conservative).
    """
    b_calls = _collect_func_calls_in_cond(b_ast)
    a_calls = _collect_func_calls_in_cond(a_ast)

    # Build maps: callee -> set of (arity, args_text_tuple)
    from collections import defaultdict
    bm = defaultdict(set)
    am = defaultdict(set)
    for c in b_calls: bm[c[0]].add((c[1], c[2]))
    for c in a_calls: am[c[0]].add((c[1], c[2]))

    # Any callee only appears on one side?
    only_in_b = set(bm.keys()) - set(am.keys())
    only_in_a = set(am.keys()) - set(bm.keys())
    if only_in_b or only_in_a:
        return True, f"callee set differs (removed={sorted(only_in_b)}, added={sorted(only_in_a)})", {
            "before_calls": b_calls, "after_calls": a_calls
        }

    # Same callee present; check signatures
    for callee in bm.keys():
        if bm[callee] != am[callee]:
            # arity or args differ
            return True, f"call signature change for '{callee}'", {
                "before": sorted(bm[callee]),
                "after":  sorted(am[callee])
            }

    return False, "no function-call change", {"before_calls": b_calls, "after_calls": a_calls}

def should_skip_smt_due_to_func_change(cond_b, cond_a):
    """
    True ⇒ do NOT send to SMT because function calls inside the condition changed.
    """
    changed, reason, _ = functions_changed_between(cond_b, cond_a)
    return changed, reason

####################tree_sitter adaptation###########################
"""
ts_adapter.py — Tree-sitter → pycparser c_ast adapter
=======================================================
Converts a tree-sitter C parse tree into real pycparser c_ast node
instances so that SMTFormulaBuilder, build_cfg, attach_parents, and
annotate_cfg_with_conditions all work completely unchanged — every
isinstance() check and NodeVisitor dispatch continues to work.

Usage (drop-in for pycparser.parse_file):
------------------------------------------
    # Old:
    from pycparser import parse_file
    ast = parse_file(c_file_path, use_cpp=True)

    # New:
    from ts_adapter import parse_c_file, parse_c_snippet
    ast = parse_c_file(c_file_path)       # reads from disk
    ast = parse_c_snippet(source_string)  # for in-memory strings

Install:
    pip install tree-sitter tree-sitter-c
"""

import tree_sitter_c as tsc
from tree_sitter import Language, Parser
from tree_sitter import Node as TSNode

from pycparser import c_ast
from pycparser.c_ast import (
    FileAST, FuncDef, Compound,
    If, While, DoWhile, For, Switch, Case, Default,
    Return, Break, Continue, Goto, Label,
    BinaryOp, UnaryOp, ID, Constant,
    FuncCall, ExprList,
    Assignment, Decl, TernaryOp,
    Cast, ArrayRef, StructRef,
    TypeDecl, IdentifierType, PtrDecl,
)

# ── Parser setup ───────────────────────────────────────────────────────────────
C_LANGUAGE = Language(tsc.language())
_parser = Parser(C_LANGUAGE)


# ── Public API ─────────────────────────────────────────────────────────────────

def parse_c_file(path: str) -> FileAST:
    """
    Read a C file from disk and return a pycparser FileAST.
    Works on incomplete/macro-heavy/platform-specific code that
    pycparser and libclang would reject.
    """
    with open(path, "rb") as f:
        source_bytes = f.read()
    return _parse_bytes(source_bytes)


def parse_c_snippet(source: str) -> FileAST:
    """Parse an in-memory C string and return a pycparser FileAST."""
    return _parse_bytes(source.encode("utf-8"))


def get_parse_errors(path: str) -> list[dict]:
    """
    Return a list of {line, col, text} dicts for ERROR/MISSING nodes.
    Unlike libclang severity-3 errors, these never abort parsing —
    the rest of the tree is always valid.
    """
    with open(path, "rb") as f:
        source_bytes = f.read()
    tree = _parser.parse(source_bytes)
    errors = []
    _collect_errors(tree.root_node, source_bytes, errors)
    return errors


# ── Internal ───────────────────────────────────────────────────────────────────

import re as _re

# Matches a function signature with NO return type — i.e. the very first
# non-whitespace token on the first non-empty line is directly the function
# name (identifier) followed immediately by '('.
#
# Real-world causes in CVE datasets:
#   • FreeType macros stripped: "FT_Bitmap_Copy( FT_Library lib, ...)"
#   • Android stripped macros:  "Java_foo_bar(JNIEnv *env, ...)"
#   • K&R implicit-int style:   "old_func(a, b)"
#
# Strategy: if the first meaningful content looks like "Identifier(" with
# no preceding C type keyword or storage class, prepend "int " so that
# tree-sitter recognises it as a function_definition.

_TYPE_KEYWORDS = {
    'void','int','char','short','long','float','double','unsigned','signed',
    'struct','union','enum','const','static','inline','extern','register',
    'volatile','auto','typedef','__attribute__','__inline__','__forceinline',
}

def _preprocess_source(source_bytes: bytes) -> bytes:
    """
    Detect and fix the most common reason tree-sitter fails to find a
    function_definition: the return type has been stripped by macro expansion
    or the dataset preprocessing step.

    We only patch when the FIRST non-blank, non-comment token looks like a
    bare identifier immediately followed by '(' — meaning there is no return
    type at all.  We prepend 'int ' which is enough for tree-sitter to
    recognise the construct as a function_definition.

    This is purely a syntactic hint to the parser; it has no effect on the
    CFG or SMT analysis because build_cfg never inspects return types.
    """
    text = source_bytes.decode('utf-8', errors='replace')

    # Find the first non-empty, non-comment line
    first_token_re = _re.compile(
        r'^\s*'               # leading whitespace
        r'([A-Za-z_]\w*)'     # group 1: first identifier
        r'\s*\(',             # immediately followed by '('
        _re.MULTILINE
    )
    m = first_token_re.search(text)
    if m is None:
        return source_bytes   # no function-like pattern found, leave as-is

    first_id = m.group(1)

    # If the first identifier IS a known type keyword, the return type is
    # already present — no patch needed.
    if first_id in _TYPE_KEYWORDS:
        return source_bytes

    # Check nothing before the identifier on the same logical "declaration"
    # that looks like a type — e.g. "static int foo(" should not be patched.
    prefix = text[:m.start(1)]
    # Strip whitespace and common non-type noise; if any type keyword remains
    # in the same "declaration context" (same block before the identifier),
    # don't patch.
    prefix_tokens = _re.findall(r'[A-Za-z_]\w*', prefix)
    # Only look at tokens after the last '}' or ';' (i.e. same declaration)
    last_boundary = max(
        text.rfind('}', 0, m.start()),
        text.rfind(';', 0, m.start()),
    )
    context_text = text[last_boundary + 1 : m.start(1)]
    context_tokens = set(_re.findall(r'[A-Za-z_]\w*', context_text))

    if context_tokens & _TYPE_KEYWORDS:
        return source_bytes   # a type keyword precedes the identifier

    # Patch: insert 'int ' right before the first identifier
    patched = text[:m.start(1)] + 'int ' + text[m.start(1):]
    return patched.encode('utf-8')


def _parse_bytes(source_bytes: bytes) -> FileAST:
    source_bytes = _preprocess_source(source_bytes)
    tree = _parser.parse(source_bytes)
    converter = _Converter(source_bytes)
    return converter.convert_translation_unit(tree.root_node)


def _collect_errors(node: TSNode, source: bytes, out: list):
    if node.type == "ERROR":
        out.append({
            "line": node.start_point[0] + 1,
            "col":  node.start_point[1],
            "text": source[node.start_byte:node.end_byte]
                        .decode("utf-8", errors="replace")
                        .replace("\n", "\\n")[:80],
            "kind": "ERROR",
        })
    if node.is_missing:
        out.append({
            "line": node.start_point[0] + 1,
            "col":  node.start_point[1],
            "text": f"missing '{node.type}'",
            "kind": "MISSING",
        })
    for child in node.children:
        _collect_errors(child, source, out)


# ── Converter ──────────────────────────────────────────────────────────────────

class _Converter:
    """
    Recursively walks a tree-sitter C AST and emits real pycparser
    c_ast node instances.

    Design rules:
      1. Every method returns a pycparser c_ast node (never None at the
         top level) so downstream isinstance() checks always succeed.
      2. For nodes we cannot fully model we fall back to an ID whose name
         encodes the raw source text — SMTFormulaBuilder's generic_visit /
         _fallback_symbol handles it as an uninterpreted Int, which is the
         correct safe degradation.
      3. Global / undeclared variables are NOT dropped.  They appear as
         ID nodes exactly like any other identifier, so _fallback_symbol
         picks them up and adds them as uninterpreted Z3 Int variables.
    """

    def __init__(self, source: bytes):
        self.src = source

    # ── text helpers ───────────────────────────────────────────────────────

    def _t(self, node: TSNode) -> str:
        """Raw source text of a node."""
        return self.src[node.start_byte:node.end_byte].decode("utf-8", errors="replace")

    def _fallback_id(self, node: TSNode) -> ID:
        """
        Safe fallback for any unrecognised node.
        The mangled name is stable so the SMTFormulaBuilder cache works.
        """
        raw  = self._t(node).replace("\n", " ").strip()[:40]
        safe = "unk_" + "".join(c if (c.isalnum() or c == "_") else "_" for c in raw)
        return ID(name=safe)

    def _dummy_type(self, name: str = None) -> TypeDecl:
        """Minimal TypeDecl used wherever build_cfg doesn't inspect types."""
        return TypeDecl(
            declname=name, quals=[], align=None,
            type=IdentifierType(names=["int"]),
        )

    # ── top-level dispatcher ───────────────────────────────────────────────

    def conv(self, node: TSNode):
        """
        Main dispatch table.  Keeps a flat if/elif chain so it is easy to
        add new node types without touching existing ones.
        """
        if node is None:
            return None

        t = node.type

        # ── tree-sitter inserts ERROR nodes in place of malformed text.
        #    We do NOT abort — we emit a fallback ID so the CFG still
        #    gets a node and path condition analysis continues.
        if t == "ERROR":
            return self._fallback_id(node)

        # ── top-level declarations
        if t == "translation_unit":         return self.convert_translation_unit(node)
        if t == "function_definition":      return self._function_def(node)
        if t == "declaration":              return self._declaration(node)
        if t == "type_definition":          return self._typedef(node)
        if t == "preproc_include":          return None   # drop preprocessor noise
        if t == "preproc_def":              return None
        if t == "preproc_ifdef":            return None
        if t == "preproc_if":              return None
        if t == "comment":                  return None

        # ── statements
        if t == "compound_statement":       return self._compound(node)
        if t == "if_statement":             return self._if(node)
        if t == "while_statement":          return self._while(node)
        if t == "do_statement":             return self._do_while(node)
        if t == "for_statement":            return self._for(node)
        if t == "switch_statement":         return self._switch(node)
        if t == "case_statement":           return self._case(node)
        if t == "return_statement":         return self._return(node)
        if t == "break_statement":          return Break()
        if t == "continue_statement":       return Continue()
        if t == "goto_statement":           return self._goto(node)
        if t == "labeled_statement":        return self._label(node)
        if t == "expression_statement":     return self._expr_stmt(node)

        # ── expressions
        if t == "binary_expression":        return self._binary(node)
        if t == "unary_expression":         return self._unary(node)
        if t == "pointer_expression":       return self._pointer_expr(node)
        if t == "update_expression":        return self._update(node)
        if t == "call_expression":          return self._call(node)
        if t == "assignment_expression":    return self._assignment(node)
        if t == "conditional_expression":   return self._ternary(node)
        if t == "subscript_expression":     return self._subscript(node)
        if t == "field_expression":         return self._field(node)
        if t == "cast_expression":          return self._cast(node)
        if t == "sizeof_expression":        return self._sizeof(node)
        if t == "parenthesized_expression": return self._paren(node)
        if t == "comma_expression":         return self._comma(node)

        # ── atoms
        if t == "identifier":               return ID(name=self._t(node))
        if t == "number_literal":           return Constant("int",    self._t(node))
        if t == "float_literal":            return Constant("float",  self._t(node))
        if t == "string_literal":           return Constant("string", self._t(node))
        if t == "concatenated_string":      return Constant("string", self._t(node))
        if t == "char_literal":             return Constant("char",   self._t(node))
        if t == "null":                     return Constant("int",    "NULL")
        if t == "true":                     return Constant("int",    "1")
        if t == "false":                    return Constant("int",    "0")

        # ── anything else (platform macros, __attribute__, etc.)
        return self._fallback_id(node)

    # ── translation unit ───────────────────────────────────────────────────

    def convert_translation_unit(self, node: TSNode) -> FileAST:
        ext = []
        for child in node.named_children:
            if child.type == "ERROR":
                # tree-sitter wraps unparseable regions (macros, __attribute__,
                # platform-specific storage classes, etc.) in ERROR nodes.
                # The actual function_definition is often a child of that ERROR
                # node — we must descend into it rather than skip it.
                self._rescue_from_error(child, ext)
            else:
                converted = self.conv(child)
                if converted is not None:
                    ext.append(converted)
        return FileAST(ext=ext)

    def _rescue_from_error(self, node: TSNode, out: list):
        """
        Recursively walk an ERROR node tree looking for function_definition
        nodes that tree-sitter managed to parse despite the surrounding error.
        This is the primary reason files show 'No function definition found':
        a macro or attribute before the function signature confuses the grammar,
        producing ERROR(...function_definition(...)) instead of a bare
        function_definition at the translation-unit level.
        """
        for child in node.children:   # use .children not .named_children
                                       # so we don't miss anonymous wrappers
            if child.type == "function_definition":
                converted = self._function_def(child)
                if converted is not None:
                    out.append(converted)
            elif child.type == "declaration":
                converted = self._declaration(child)
                if converted is not None:
                    out.append(converted)
            elif child.has_error or child.type == "ERROR":
                # Keep descending — the real node may be deeper
                self._rescue_from_error(child, out)

    # ── function definition ────────────────────────────────────────────────

    def _function_def(self, node: TSNode) -> FuncDef:
        body_node = node.child_by_field_name("body")
        body      = self._compound(body_node) if body_node else Compound(block_items=[])

        declarator = node.child_by_field_name("declarator")
        func_name  = self._extract_func_name(declarator)

        decl = Decl(
            name=func_name, quals=[], align=[], storage=[], funcspec=[],
            type=self._dummy_type(func_name),
            init=None, bitsize=None,
        )
        return FuncDef(decl=decl, param_decls=None, body=body)

    def _extract_func_name(self, node: TSNode) -> str:
        """Walk declarator subtree to find the identifier (function name)."""
        if node is None:
            return "unknown"
        if node.type == "identifier":
            return self._t(node)
        # function_declarator, pointer_declarator, etc.
        inner = node.child_by_field_name("declarator")
        if inner:
            return self._extract_func_name(inner)
        # scan named children as fallback
        for child in node.named_children:
            if child.type == "identifier":
                return self._t(child)
            result = self._extract_func_name(child)
            if result != "unknown":
                return result
        return "unknown"

    # ── compound statement ─────────────────────────────────────────────────

    def _compound(self, node: TSNode) -> Compound:
        if node is None:
            return Compound(block_items=None)
        items = []
        for child in node.named_children:
            converted = self.conv(child)
            if converted is not None:
                items.append(converted)
        return Compound(block_items=items or None)

    def _ensure_compound(self, node: TSNode) -> Compound:
        """Guarantee a Compound — wraps single statements automatically."""
        if node is None:
            return Compound(block_items=None)
        converted = self.conv(node)
        if isinstance(converted, Compound):
            return converted
        return Compound(block_items=[converted] if converted is not None else None)

    # ── if ─────────────────────────────────────────────────────────────────

    def _if(self, node: TSNode) -> If:
        cond_node = node.child_by_field_name("condition")
        then_node = node.child_by_field_name("consequence")
        else_node = node.child_by_field_name("alternative")

        cond   = self._unwrap_paren(cond_node)
        iftrue = self._ensure_compound(then_node)

        if else_node is not None:
            # tree-sitter wraps else body in an `else_clause` node
            # whose first named child is the actual body/if
            inner = (else_node.named_children[0]
                     if else_node.named_children else else_node)
            if inner.type == "if_statement":
                iffalse = self._if(inner)
            else:
                iffalse = self._ensure_compound(inner)
        else:
            iffalse = None

        return If(cond=cond, iftrue=iftrue, iffalse=iffalse)

    # ── while ──────────────────────────────────────────────────────────────

    def _while(self, node: TSNode) -> While:
        return While(
            cond=self._unwrap_paren(node.child_by_field_name("condition")),
            stmt=self._ensure_compound(node.child_by_field_name("body")),
        )

    # ── do-while ───────────────────────────────────────────────────────────

    def _do_while(self, node: TSNode) -> DoWhile:
        return DoWhile(
            cond=self._unwrap_paren(node.child_by_field_name("condition")),
            stmt=self._ensure_compound(node.child_by_field_name("body")),
        )

    # ── for ────────────────────────────────────────────────────────────────

    def _for(self, node: TSNode) -> For:
        init_node   = node.child_by_field_name("initializer")
        cond_node   = node.child_by_field_name("condition")
        update_node = node.child_by_field_name("update")
        body_node   = node.child_by_field_name("body")

        return For(
            init=self.conv(init_node)   if init_node   else None,
            cond=self.conv(cond_node)   if cond_node   else None,
            next=self.conv(update_node) if update_node else None,
            stmt=self._ensure_compound(body_node),
        )

    # ── switch ─────────────────────────────────────────────────────────────

    def _switch(self, node: TSNode) -> Switch:
        return Switch(
            cond=self._unwrap_paren(node.child_by_field_name("condition")),
            stmt=self._compound(node.child_by_field_name("body")),
        )

    # ── case / default ─────────────────────────────────────────────────────

    def _case(self, node: TSNode) -> c_ast.Node:
        """
        tree-sitter uses `case_statement` for both `case X:` and `default:`.
        The presence of a `value` field distinguishes them.
        """
        value_node = node.child_by_field_name("value")

        # Collect body statements (everything after the value/colon)
        stmts = []
        for child in node.named_children:
            if child is value_node:
                continue
            converted = self.conv(child)
            if converted is not None:
                stmts.append(converted)

        if value_node is None:
            # default:
            return Default(stmts=stmts or None)
        else:
            return Case(expr=self.conv(value_node), stmts=stmts or None)

    # ── return ─────────────────────────────────────────────────────────────

    def _return(self, node: TSNode) -> Return:
        children = node.named_children
        expr = self.conv(children[0]) if children else None
        return Return(expr=expr)

    # ── goto / label ───────────────────────────────────────────────────────

    def _goto(self, node: TSNode) -> Goto:
        label_node = node.child_by_field_name("label")
        return Goto(name=self._t(label_node) if label_node else "unknown")

    def _label(self, node: TSNode) -> Label:
        label_node = node.child_by_field_name("label")
        body_node  = node.child_by_field_name("body")
        return Label(
            name=self._t(label_node) if label_node else "unknown",
            stmt=self.conv(body_node) if body_node else None,
        )

    # ── expression statement ───────────────────────────────────────────────

    def _expr_stmt(self, node: TSNode) -> c_ast.Node:
        """
        Unwrap the expression_statement shell — return the inner expression.
        build_cfg's process_statement handles raw expressions fine.
        """
        children = node.named_children
        return self.conv(children[0]) if children else None

    # ── declaration ────────────────────────────────────────────────────────

    def _declaration(self, node: TSNode) -> Decl:
        """
        We only need Decl.name and Decl.init for the existing code:
          - build_cfg checks isinstance(stmt.init, TernaryOp)
          - SMTFormulaBuilder never inspects type information
        Everything else is set to a safe dummy value.
        """
        name = "unknown"
        init = None

        for child in node.named_children:
            if child.type == "init_declarator":
                decl_part = child.child_by_field_name("declarator")
                val_part  = child.child_by_field_name("value")
                if decl_part:
                    name = self._extract_decl_name(decl_part)
                if val_part:
                    init = self.conv(val_part)

            elif child.type == "identifier":
                name = self._t(child)

            elif child.type in ("pointer_declarator", "array_declarator",
                                "function_declarator"):
                name = self._extract_decl_name(child)

        return Decl(
            name=name, quals=[], align=[], storage=[], funcspec=[],
            type=self._dummy_type(name),
            init=init, bitsize=None,
        )

    def _typedef(self, node: TSNode):
        """typedef — not needed for CFG/SMT, return None to skip silently."""
        return None

    def _extract_decl_name(self, node: TSNode) -> str:
        if node is None:
            return "unknown"
        if node.type == "identifier":
            return self._t(node)
        inner = node.child_by_field_name("declarator")
        if inner:
            return self._extract_decl_name(inner)
        for child in node.named_children:
            result = self._extract_decl_name(child)
            if result != "unknown":
                return result
        return "unknown"

    # ── binary expression ──────────────────────────────────────────────────

    def _binary(self, node: TSNode) -> BinaryOp:
        left_node  = node.child_by_field_name("left")
        op_node    = node.child_by_field_name("operator")
        right_node = node.child_by_field_name("right")

        left  = self.conv(left_node)  if left_node  else self._fallback_id(node)
        right = self.conv(right_node) if right_node else self._fallback_id(node)
        op    = self._t(op_node)      if op_node    else "+"

        return BinaryOp(op=op, left=left, right=right)

    # ── unary expression ───────────────────────────────────────────────────

    def _unary(self, node: TSNode) -> UnaryOp:
        """
        tree-sitter unary_expression: operator + argument fields.
        Handles !, ~, - (negation), + (no-op).
        """
        op_node  = node.child_by_field_name("operator")
        arg_node = node.child_by_field_name("argument")
        op  = self._t(op_node)  if op_node  else "!"
        arg = self.conv(arg_node) if arg_node else self._fallback_id(node)
        return UnaryOp(op=op, expr=arg)

    def _pointer_expr(self, node: TSNode) -> UnaryOp:
        """
        tree-sitter pointer_expression covers * (deref) and & (address-of).
        Maps directly to pycparser UnaryOp with op '*' or '&'.
        """
        op_node  = node.child_by_field_name("operator")
        arg_node = node.child_by_field_name("argument")
        op  = self._t(op_node)    if op_node  else "*"
        arg = self.conv(arg_node) if arg_node else self._fallback_id(node)
        return UnaryOp(op=op, expr=arg)

    def _update(self, node: TSNode) -> UnaryOp:
        """
        tree-sitter update_expression: ++x (prefix) or x++ (postfix).
        pycparser convention: 'p++' / 'p--' for postfix, '++' / '--' for prefix.
        """
        op_node  = node.child_by_field_name("operator")
        arg_node = node.child_by_field_name("argument")
        op  = self._t(op_node)    if op_node  else "++"
        arg = self.conv(arg_node) if arg_node else self._fallback_id(node)

        # Postfix: argument byte-range comes before operator byte-range
        is_postfix = (
            arg_node is not None and op_node is not None
            and arg_node.start_byte < op_node.start_byte
        )
        if is_postfix:
            op = "p" + op   # 'p++' or 'p--'

        return UnaryOp(op=op, expr=arg)

    # ── call expression ────────────────────────────────────────────────────

    def _call(self, node: TSNode) -> FuncCall:
        func_node = node.child_by_field_name("function")
        args_node = node.child_by_field_name("arguments")

        name = self.conv(func_node) if func_node else ID(name="unknown")

        if args_node is None:
            args = None
        else:
            arg_exprs = [
                self.conv(c)
                for c in args_node.named_children
                if c.type not in ("comment",)
            ]
            arg_exprs = [a for a in arg_exprs if a is not None]
            args = ExprList(exprs=arg_exprs) if arg_exprs else None

        return FuncCall(name=name, args=args)

    # ── assignment expression ──────────────────────────────────────────────

    def _assignment(self, node: TSNode) -> Assignment:
        left_node  = node.child_by_field_name("left")
        op_node    = node.child_by_field_name("operator")
        right_node = node.child_by_field_name("right")

        return Assignment(
            op=self._t(op_node)       if op_node    else "=",
            lvalue=self.conv(left_node)  if left_node  else self._fallback_id(node),
            rvalue=self.conv(right_node) if right_node else self._fallback_id(node),
        )

    # ── ternary / conditional ──────────────────────────────────────────────

    def _ternary(self, node: TSNode) -> TernaryOp:
        cond_node  = node.child_by_field_name("condition")
        true_node  = node.child_by_field_name("consequence")
        false_node = node.child_by_field_name("alternative")

        return TernaryOp(
            cond=self.conv(cond_node)   if cond_node  else Constant("int", "1"),
            iftrue=self.conv(true_node) if true_node  else Constant("int", "0"),
            iffalse=self.conv(false_node) if false_node else Constant("int", "0"),
        )

    # ── subscript / array ref ──────────────────────────────────────────────

    def _subscript(self, node: TSNode) -> ArrayRef:
        arg_node   = node.child_by_field_name("argument")
        index_node = node.child_by_field_name("index")
        return ArrayRef(
            name=self.conv(arg_node)       if arg_node   else self._fallback_id(node),
            subscript=self.conv(index_node) if index_node else Constant("int", "0"),
        )

    # ── field expression (-> and .) ────────────────────────────────────────

    def _field(self, node: TSNode) -> StructRef:
        arg_node   = node.child_by_field_name("argument")
        field_node = node.child_by_field_name("field")
        op_node    = node.child_by_field_name("operator")
        return StructRef(
            name=self.conv(arg_node)    if arg_node   else self._fallback_id(node),
            type=self._t(op_node)       if op_node    else "->",
            field=self.conv(field_node) if field_node else ID(name="unknown"),
        )

    # ── cast ───────────────────────────────────────────────────────────────

    def _cast(self, node: TSNode) -> Cast:
        type_node = node.child_by_field_name("type")
        val_node  = node.child_by_field_name("value")
        expr      = self.conv(val_node) if val_node else self._fallback_id(node)

        if type_node is not None:
            raw_type  = self._t(type_node).strip()
            base_type = raw_type.rstrip('*').strip()
            names     = base_type.split() if base_type else ['int']
            to_type   = TypeDecl(
                declname=None, quals=[], align=None,
                type=IdentifierType(names=names),
            )
        else:
            to_type = self._dummy_type()

        return Cast(to_type=to_type, expr=expr)
    # ── sizeof ─────────────────────────────────────────────────────────────

    def _sizeof(self, node: TSNode) -> UnaryOp:
        # Treat sizeof(x) as an opaque Int — SMTFormulaBuilder's
        # generic_visit will assign it a fresh uninterpreted symbol.
        val_node = node.child_by_field_name("value")
        inner    = self.conv(val_node) if val_node else self._fallback_id(node)
        return UnaryOp(op="sizeof", expr=inner)

    # ── parenthesized expression ───────────────────────────────────────────

    def _paren(self, node: TSNode):
        children = node.named_children
        return self.conv(children[0]) if children else self._fallback_id(node)

    # ── comma expression (e.g. multi-init in for) ──────────────────────────

    def _comma(self, node: TSNode) -> ExprList:
        exprs = [self.conv(c) for c in node.named_children]
        return ExprList(exprs=[e for e in exprs if e is not None])

    # ── helpers ────────────────────────────────────────────────────────────

    def _unwrap_paren(self, node: TSNode):
        """
        tree-sitter wraps if/while conditions in parenthesized_expression.
        Unwrap it so the inner expression is what gets stored as ast_node
        in build_cfg — otherwise CGenerator produces double parentheses.
        """
        if node is None:
            return Constant("int", "1")
        if node.type == "parenthesized_expression":
            children = node.named_children
            if children:
                return self.conv(children[0])
        return self.conv(node)
###############################################

if __name__ == "__main__":
    main()
