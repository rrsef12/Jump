"""Small current naming scheme for tests concerned with save adapters."""
import configparser

def pack(values):
    return ''.join(str(len(v))+':'+v for v in values)

def encode(value):return value.encode('utf-16-be').hex().upper()

def install(path):
    config=configparser.ConfigParser();config.read(path,encoding='utf-16')
    count=config.getint('Jump','recipes',fallback=0)
    meta=pack(['','','','','','','1','0','0','0','0','0'])
    field=pack(['subject','主要名称','','1','0',meta]+[pack([v,v,'']) for v in ['角色设定','角色参考','场景设定']])
    structure=pack(['JNS2','adapter-fixture','适配器测试','_','1','0','0','0','3','0','0',pack(['subject','0','']),field])
    config['Recipe'+str(count)]={'name':encode('适配器测试'),'value':encode('Asset'),
        'namingOnly':'1','complete':'1','structure':encode(structure)}
    config['Jump']['recipes']=str(count+1);config['Jump']['activeNamingScheme']=encode('adapter-fixture')
    config['Jump']['defaultNamingScheme']=encode('adapter-fixture')
    with path.open('w',encoding='utf-16') as file:config.write(file)
