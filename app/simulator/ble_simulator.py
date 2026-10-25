#!/usr/bin/env python3
"""Real BLE peripheral using the system BlueZ, dbus-python and GLib."""
import argparse
import logging
import signal
import sys
from collections import deque
from protocol import SERVICE, RX, TX, Decoder, Server


def main():
    parser=argparse.ArgumentParser(description='Montre Dive Watch simulée sur Bluetooth réel')
    parser.add_argument('--scenario',choices=['normal','gap','unknown-time','unknown-alert','active-dive','corrupt-block','disconnect'],default='normal')
    parser.add_argument('--adapter',default=None,help='hci0, hci1… (détection automatique par défaut)')
    parser.add_argument('--fragment-size',type=int,default=20,choices=range(1,245),metavar='1..244')
    parser.add_argument('--duration',type=int,default=0,help='arrêt automatique après N secondes')
    args=parser.parse_args()
    try:
        import dbus
        import dbus.service
        import dbus.mainloop.glib
        from gi.repository import GLib
    except ImportError:
        parser.exit(1,'Installer les dépendances système python3-dbus et python3-gobject.\n')
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    logging.basicConfig(level=logging.INFO,format='%(message)s')
    bus=dbus.SystemBus()
    props='org.freedesktop.DBus.Properties';objmanager='org.freedesktop.DBus.ObjectManager'
    svc='org.bluez.GattService1';char='org.bluez.GattCharacteristic1';adv='org.bluez.LEAdvertisement1'
    root='/fr/divewatch/simulator';server=Server(args.scenario);decoder=Decoder();queue=deque()
    managed=dbus.Interface(bus.get_object('org.bluez','/'),objmanager).GetManagedObjects()
    adapters=[str(p) for p,ifs in managed.items() if 'org.bluez.GattManager1' in ifs and 'org.bluez.LEAdvertisingManager1' in ifs]
    if args.adapter:adapters=[p for p in adapters if p.endswith('/'+args.adapter)]
    if not adapters:parser.exit(1,'Aucun adaptateur avec GATT et advertising disponible.\n')
    adapter=adapters[0]
    if not bool(managed[adapter]['org.bluez.Adapter1']['Powered']):parser.exit(1,'Activer le Bluetooth avant de démarrer.\n')

    class Properties(dbus.service.Object):
        @dbus.service.method(props,in_signature='s',out_signature='a{sv}')
        def GetAll(self,interface):
            if interface!=self.interface:raise dbus.exceptions.DBusException('Interface inconnue')
            return self.properties()
        @dbus.service.signal(props,signature='sa{sv}as')
        def PropertiesChanged(self,interface,changed,invalidated):pass

    class Application(dbus.service.Object):
        @dbus.service.method(objmanager,out_signature='a{oa{sa{sv}}}')
        def GetManagedObjects(self):return {x.path:{x.interface:x.properties()} for x in [service,rx,tx]}

    class Service(Properties):
        interface=svc;path=root+'/service0'
        def __init__(self):super().__init__(bus,self.path)
        def properties(self):return {'UUID':SERVICE,'Primary':dbus.Boolean(True)}

    class Characteristic(Properties):
        interface=char
        def __init__(self,index,uuid,flags):
            self.path=root+'/service0/char'+str(index);self.uuid=uuid;self.flags=flags
            super().__init__(bus,self.path)
        def properties(self):
            return {'UUID':self.uuid,'Service':dbus.ObjectPath(service.path),'Flags':dbus.Array(self.flags,signature='s')}

    class Receive(Characteristic):
        def __init__(self):super().__init__(0,RX,['write'])
        @dbus.service.method(char,in_signature='aya{sv}',out_signature='')
        def WriteValue(self,value,options):
            if not tx.notifying:raise dbus.exceptions.DBusException('Activer les notifications TX avant les commandes')
            if int(options.get('offset',0))!=0:raise dbus.exceptions.DBusException('Offset non supporté')
            try:
                for frame in decoder.feed(bytes(value)):
                    reply=server.reply(frame).encode()
                    if args.scenario=='disconnect' and frame.opcode==5 and server.blocks_sent==2:
                        device=options.get('device')
                        if device:GLib.idle_add(lambda: dbus.Interface(bus.get_object('org.bluez',device),'org.bluez.Device1').Disconnect())
                        queue.clear();return
                    mtu=int(options.get('mtu',23))
                    limit=min(args.fragment_size,max(1,mtu-3))
                    queue.extend(reply[i:i+limit] for i in range(0,len(reply),limit))
            except ValueError as error:
                decoder.pending.clear();queue.clear();raise dbus.exceptions.DBusException(str(error))

    class Transmit(Characteristic):
        def __init__(self):super().__init__(1,TX,['notify']);self.notifying=False
        def properties(self):return dict(super().properties(),Notifying=dbus.Boolean(self.notifying),Value=dbus.Array([],signature='y'))
        @dbus.service.method(char)
        def StartNotify(self):
            self.notifying=True;logging.info('Notifications activées : téléphone connecté')
            self.PropertiesChanged(char,{'Notifying':dbus.Boolean(True)},[])
        @dbus.service.method(char)
        def StopNotify(self):
            self.notifying=False;queue.clear();decoder.pending.clear()
            self.PropertiesChanged(char,{'Notifying':dbus.Boolean(False)},[])

    class Advertisement(Properties):
        interface=adv;path=root+'/advertisement0'
        def __init__(self):super().__init__(bus,self.path)
        def properties(self):return {'Type':'peripheral','ServiceUUIDs':dbus.Array([SERVICE],signature='s'),'LocalName':'DiveWatch-SIM'}
        @dbus.service.method(adv)
        def Release(self):logging.info('Annonce libérée')

    application=Application(bus,root);service=Service();rx=Receive();tx=Transmit();advertisement=Advertisement()
    loop=GLib.MainLoop();gatt=dbus.Interface(bus.get_object('org.bluez',adapter),'org.bluez.GattManager1')
    advertising=dbus.Interface(bus.get_object('org.bluez',adapter),'org.bluez.LEAdvertisingManager1')
    registered=[]
    def failure(error):logging.error('BlueZ : %s',error);loop.quit()
    def advertised():
        registered.append('advertisement');logging.info('DiveWatch-SIM prêt sur %s — scénario %s',adapter,args.scenario)
    def application_ready():
        registered.append('application');advertising.RegisterAdvertisement(advertisement.path,{},reply_handler=advertised,error_handler=failure)
    def send():
        if tx.notifying and queue:
            data=queue.popleft();tx.PropertiesChanged(char,{'Value':dbus.Array(data,signature='y')},[])
        return True
    def shutdown(*_):loop.quit();return False
    GLib.timeout_add(10,send)
    if args.duration:GLib.timeout_add_seconds(args.duration,shutdown)
    signal.signal(signal.SIGINT,shutdown);signal.signal(signal.SIGTERM,shutdown)
    gatt.RegisterApplication(root,{},reply_handler=application_ready,error_handler=failure)
    try:loop.run()
    finally:
        if 'advertisement' in registered:advertising.UnregisterAdvertisement(advertisement.path)
        if 'application' in registered:gatt.UnregisterApplication(root)
    return 0 if len(registered)==2 else 1

if __name__=='__main__':sys.exit(main())
