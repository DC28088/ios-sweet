#!/usr/bin/env python3
"""Receive the sweet kernel console over CDC-ECM; no host sudo when NM permits it."""
import argparse,json,pathlib,socket,subprocess,sys,time
MAC='02:00:5e:77:ee:01'
PROFILE='sweet-usb-log'
def run(*args):
 return subprocess.run(args,check=True,stdout=sys.stderr)
def interface():
 links=json.loads(subprocess.check_output(['ip','-j','link'],text=True))
 return next((link['ifname'] for link in links if link.get('address','').lower()==MAC),None)
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('logfile',nargs='?')
 parser.add_argument('--duration',type=float,help='Stop after this many seconds; default until Ctrl-C')
 args=parser.parse_args()
 if args.duration is not None and args.duration<=0:parser.error('duration must be positive')
 # Listen BEFORE configuring the link so its first routed packets are captured.
 sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
 try:sock.bind(('0.0.0.0',5140))
 except OSError as error:raise SystemExit(f'Cannot listen on UDP5140: {error}. Stop the other receiver first.')
 sock.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1048576)
 sock.settimeout(1)
 out=None
 if args.logfile:
  path=pathlib.Path(args.logfile);path.parent.mkdir(parents=True,exist_ok=True);out=path.open('ab',buffering=0)
 start=time.monotonic();configured=None;total=0;last_notice=start
 print('Listening on UDP5140; waiting for sweet USB Ethernet '+MAC,file=sys.stderr,flush=True)
 try:
  while args.duration is None or time.monotonic()-start<args.duration:
   name=interface()
   if name and name!=configured:
    profiles=subprocess.check_output(['nmcli','--escape','no','-g','NAME','connection','show'],text=True).splitlines()
    if PROFILE not in profiles:
     run('nmcli','connection','add','type','ethernet','con-name',PROFILE,'ifname','*',
         '802-3-ethernet.mac-address',MAC,'ipv4.method','manual','ipv4.addresses','192.168.7.1/24',
         'ipv4.never-default','yes','ipv6.method','disabled','connection.autoconnect','yes',
         'connection.autoconnect-priority','100')
    bound=subprocess.check_output(['nmcli','--escape','no','-g','802-3-ethernet.mac-address','connection','show',PROFILE],text=True).strip().lower()
    if bound!=MAC:raise RuntimeError('Existing sweet-usb-log profile has an unexpected MAC; inspect it before proceeding')
    current=subprocess.check_output(['nmcli','--escape','no','-g','GENERAL.CONNECTION','device','show',name],text=True).strip()
    if current!=PROFILE:run('nmcli','connection','up',PROFILE,'ifname',name)
    configured=name
    print(f'{name}: receiver ready (host192.168.7.1, phone192.168.7.2); awaiting first console packet',file=sys.stderr,flush=True)
   elif not name:configured=None
   try:data,peer=sock.recvfrom(65535)
   except socket.timeout:
    now=time.monotonic()
    if configured and not total and now-last_notice>=30:
     stats=pathlib.Path('/sys/class/net')/configured/'statistics'
     try:
      packets=(stats/'rx_packets').read_text().strip();errors=(stats/'rx_errors').read_text().strip()
     except OSError:packets=errors='unavailable'
     print(f'No console packets received; {configured} RX packets={packets}, RX errors={errors}. Link enumeration alone does not prove data transfer.',file=sys.stderr,flush=True)
     last_notice=now
    continue
   if peer[0]!='192.168.7.2':continue
   if not total:print(f'Live console verified: received {len(data)} bytes from {peer[0]}',file=sys.stderr,flush=True)
   if out:out.write(data)
   sys.stdout.buffer.write(data);sys.stdout.buffer.flush();total+=len(data)
 except KeyboardInterrupt:pass
 finally:
  sock.close()
  if out:out.close()
  print(f'\nCaptured {total} bytes',file=sys.stderr,flush=True)
if __name__=='__main__':main()
