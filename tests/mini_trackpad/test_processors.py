"""Exercise compiled-DT chains with the pinned ZMK C filter/processor functions."""
import sys,re,struct,subprocess,ctypes
from pathlib import Path
import argparse,tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--workspace', type=Path, required=True, help='west workspace containing zmk and zephyr')
parser.add_argument('--build', type=Path, required=True, help='built Central directory')
args=parser.parse_args()
base=args.workspace.resolve()
sys.path.insert(0,str(base/'zephyr/scripts/dts/python-devicetree/src'))
from devicetree import dtlib
src=base/'zmk/app/src/pointing'
temporary=tempfile.TemporaryDirectory();target=Path(temporary.name)
dt=dtlib.DT(str(args.build.resolve()/'zephyr/zephyr.dts'))
node=dt.get_node('/pointing_device_split_listener');override=node.nodes['mouse_layer']
def nums(prop):return list(struct.unpack('>'+'I'*(len(prop.value)//4),prop.value))
def chain(n):
 raw=nums(n.props['input-processors']);result=[]
 while raw:
  p=dt.phandle2node[raw.pop(0)];count=p.props['#input-processor-cells'].to_num();args=raw[:count];del raw[:count]
  
  if p.props['compatible'].to_string() != 'torabo,mini-scroll-inertia':
   result.append((p,args+[0]*(2-count)))
 return result
chains=[chain(node),chain(override)]
assert override.props['layers'].to_nums()==[6] and 'process-next' not in override.props
c='''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#define LOG_DBG(...) ((void)0)
#define LOG_MODULE_DECLARE(...)
#define DT_INST_FOREACH_STATUS_OKAY(x)
#define BIT(n) (1U << (n))
#define ZMK_INPUT_PROC_CONTINUE 0
#define ZMK_INPUT_PROC_STOP 1
struct device { const void *config; const void *api; };
struct input_event { const struct device *dev; uint8_t type; uint16_t code; int32_t value; bool sync; };
struct zmk_input_processor_state { uint8_t input_device_index; int16_t *remainder; };
struct zmk_input_processor_entry { const struct device *dev; uint32_t param1,param2; bool track_remainders; };
struct zmk_input_processor_driver_api { int (*handle_event)(const struct device *, struct input_event *, uint32_t, uint32_t, struct zmk_input_processor_state *); };
static int zmk_input_processor_handle_event(const struct device *dev, struct input_event *evt, uint32_t p1, uint32_t p2, struct zmk_input_processor_state *state) {
 return ((const struct zmk_input_processor_driver_api *)dev->api)->handle_event(dev,evt,p1,p2,state);
}
static uint32_t active_layers;
static bool zmk_keymap_layer_active(uint8_t layer) { return !!(active_layers & BIT(layer)); }
'''
c+='#include "'+str(base/'zephyr/include/zephyr/dt-bindings/input/input-event-codes.h')+'"\n'
c+='#include "'+str(base/'zmk/app/include/dt-bindings/zmk/input_transform.h')+'"\n'
for name in ['transform','code_mapper','scaler']:
 code=(src/f'input_processor_{name}.c').read_text()
 code=re.sub(r'^#include.*$', '',code,flags=re.M)
 c+='\n#undef DT_DRV_COMPAT\n'+code+'\n'
listener=(src/'input_listener.c').read_text()
c+=listener[listener.index('enum input_listener_xy_data_mode'):listener.index('static void handle_rel_code')]
c+=listener[listener.index('static int apply_config'):listener.index('static void clear_xy_data')]
ids={}
for ch in chains:
 for p,args in ch:
  if p.path in ids:continue
  i=len(ids);ids[p.path]=i;typ=p.props['type'].to_num();compat=p.props['compatible'].to_string()
  def array(k):return ','.join(map(str,p.props[k].to_nums()))
  if compat.endswith('transform'):
   c+=f'static const uint16_t xs{i}[]={{{array("x-codes")}}},ys{i}[]={{{array("y-codes")}}};\n'
   c+=f'static const struct ipt_config cfg{i}={{.type={typ},.x_codes_size=1,.y_codes_size=1,.x_codes=xs{i},.y_codes=ys{i}}};\n';api='ipt'
  elif compat.endswith('code-mapper'):
   c+=f'static const struct cm_config cfg{i}={{.type={typ},.mapping_size={len(p.props["map"].to_nums())},.mapping={{{array("map")}}}}};\n';api='cm'
  else:
   assert compat.endswith('scaler')
   c+=f'static const struct scaler_config cfg{i}={{.type={typ},.codes_len={len(p.props["codes"].to_nums())},.codes={{{array("codes")}}}}};\n';api='scaler'
  c+=f'static const struct device dev{i}={{.config=&cfg{i},.api=&{api}_driver_api}};\n'
for n,ch in enumerate(chains):
 c+=f'static struct zmk_input_processor_entry chain{n}[]={{'+','.join('{.dev=&dev%d,.param1=%d,.param2=%d,.track_remainders=%s}'%(ids[p.path],*args,'true' if 'track-remainders' in p.props else 'false') for p,args in ch)+'};\n'
c+='''static struct input_processor_remainder_data base_rem[1], mouse_rem[1];
static struct input_listener_data data={.base_processor_data={.remainders_len=1,.remainders=base_rem},.layer_override_data={{.remainders_len=1,.remainders=mouse_rem}}};
static const struct input_listener_config config={.listener_index=1,.base={.processors_len=sizeof(chain0)/sizeof(chain0[0]),.processors=chain0},.layer_overrides_len=1,.layer_overrides={{.layer_mask=BIT(6),.process_next=false,.config={.processors_len=sizeof(chain1)/sizeof(chain1[0]),.processors=chain1}}}};
void reset_state(void) { memset(base_rem,0,sizeof(base_rem));memset(mouse_rem,0,sizeof(mouse_rem)); }
void set_scale(int mul,int div) { chain0[sizeof(chain0)/sizeof(chain0[0])-1].param1=mul;chain0[sizeof(chain0)/sizeof(chain0[0])-1].param2=div;chain1[sizeof(chain1)/sizeof(chain1[0])-1].param1=mul;chain1[sizeof(chain1)/sizeof(chain1[0])-1].param2=div;reset_state(); }
int run_event(uint32_t layers, uint8_t type, uint16_t *code, int32_t *value) { active_layers=layers;struct input_event evt={.dev=&dev0,.type=type,.code=*code,.value=*value,.sync=true};int ret=filter_with_input_config(&config,&data,&evt);*code=evt.code;*value=evt.value;return ret; }
'''
(target/'processors.c').write_text(c)
subprocess.run(['gcc','-I'+str(base/'zephyr/include'),'-shared','-fPIC','-o',str(target/'processors.so'),str(target/'processors.c')],check=True)
lib=ctypes.CDLL(str(target/'processors.so'));count=0
# Codes are taken from the pinned Zephyr input header.
codes={}
for line in (base/'zephyr/include/zephyr/dt-bindings/input/input-event-codes.h').read_text().splitlines():
 m=re.match(r'#define\s+(INPUT_(?:REL_(?:X|Y|WHEEL|HWHEEL)|EV_(?:KEY|REL)|BTN_0))\s+(0x[0-9a-fA-F]+|\d+)\b',line)
 if m:codes[m[1]]=int(m[2],0)
def test(layers,kind,code,value,wantcode,wantvalue):
 global count
 codev=ctypes.c_uint16(codes[code]);val=ctypes.c_int32(value)
 assert lib.run_event(layers,codes[kind],ctypes.byref(codev),ctypes.byref(val))==0
 assert (codev.value,val.value)==(codes[wantcode],wantvalue),(layers,code,value,codev.value,val.value,wantcode,wantvalue)
 count+=1
# Inertia's direct-input passthrough is checked separately; this harness executes
# the actual pinned ZMK transform/mapper/scaler/filter with generated DT arguments.
for layers in [0,1<<1,1<<4,1<<5,(1<<4)|(1<<5),1<<6,(1<<4)|(1<<6),(1<<5)|(1<<6)]:
 mouse=bool(layers&(1<<6))
 for code,value,axis in [('INPUT_REL_X',120,'X'),('INPUT_REL_Y',-90,'Y'),('INPUT_REL_HWHEEL',120,'X'),('INPUT_REL_WHEEL',90,'Y')]:
  dest='INPUT_REL_'+(axis if mouse else {'X':'HWHEEL','Y':'WHEEL'}[axis])
  expected=(120 if axis=='X' else -90)*2//3 if mouse else (-120 if axis=='X' else 90)//2
  test(layers,'INPUT_EV_REL',code,value,dest,expected)
 for value in [1,0]:test(layers,'INPUT_EV_KEY','INPUT_BTN_0',value,'INPUT_BTN_0',value)
lib.reset_state()
# AML 4 does not change mode; only dedicated 6 does. Release works immediately.
for layers,dest,val in [(0,'INPUT_REL_HWHEEL',-6),(16,'INPUT_REL_HWHEEL',-6),(64,'INPUT_REL_X',8),(80,'INPUT_REL_X',8),(16,'INPUT_REL_HWHEEL',-6),(0,'INPUT_REL_HWHEEL',-6)]:
 test(layers,'INPUT_EV_REL','INPUT_REL_X',12,dest,val)
print(f'PASS: {count} checks: AML 4 independence, 6 entry/exit, both axes reversed only in cursor, scroll 1/2 and cursor 2/3, one/two-finger inputs, buttons.')
