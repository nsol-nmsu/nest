import os
import tkFileDialog, tkMessageBox

try:
    from Tkinter import *
except ImportError:
    from tkinter import *

from tkFileDialog import askopenfilename



class MyGUI:
    ##find path to program to run
    def find(self, name, path):
        for root, dirs, files in os.walk(path):
            if name in files:
                return os.path.join(root, name)

    def addOptions(self, topo, traceDir, ns2, duration, apps, pcap, ap, infra, rng):
        run_command = "scratch/core_to_ns3_scenario"
        if topo != "":
            run_command = run_command + " --topo=" + topo
        if apps != "":
            run_command = run_command + " --apps=" + apps
        if traceDir != "":
            run_command = run_command + " --traceDir=" + traceDir
        if ns2 != "":
            run_command = run_command + " --ns2=" + ns2
        if duration != "":
            run_command = run_command + " --duration=" + duration
        if ap != "":
            run_command = run_command + " --ap=" + ap
        if infra != "":
            run_command = run_command + " --infra=" + infra
        if pcap != "":
            run_command = run_command + " --pcap=" + pcap
        if rng != "":
            run_command = run_command + " --random=" + rng
        run_command = '"' + run_command + '"'

        waf_command = './waf --run ' + run_command

        print "\n" + waf_command + "\n"
        return waf_command

    def __init__(self, master):
        self.master = master
        path_to_file = os.path.realpath(__file__)
        path_to_dir = os.path.abspath(os.path.join(path_to_file, os.pardir))
        path_to_dir2 = os.path.abspath(os.path.join(path_to_dir, os.pardir))
        print path_to_dir2
        os.chdir(path_to_dir2)

        global rngValue
        global pcapValue
        self.ap = ""
        self.infra = ""
        self.ns2Path = ""
        self.topoPath = ""
        self.patchPath = ""
        self.outputPath = ""
        rngValue = StringVar(None)
        pcapValue = StringVar(None)
        self.durationValue = ""

        master.title("NEST")

        self.label = Label(master, text="Network Emulation to Simulation Tool (NEST)")

        L1 = Label(master, text="Select Topology").grid(row=0, column=0, sticky=W)
        L2 = Label(master, text="Select Application").grid(row=1, column=0, sticky=W)
        L3 = Label(master, text="Select Ouput Directory").grid(row=2, column=0, sticky=W)
        L4 = Label(master, text="Select Mobility File").grid(row=3, column=0, sticky=W)
        L51 = Label(master, text="Sim Duration (seconds)").grid(row=5, column=0, sticky=W)
        L52 = Label(master, text="Access Points (n1:n2:...)").grid(row=5, column=1, sticky=W)
        L53 = Label(master, text="Infrastructure Mode (wlan1:wlan2:...)").grid(row=5, column=2, sticky=W)
        L6 = Label(master, text="Run Simulation").grid(row=6, column=0, sticky=W)

        self.selectT_button = Button(master, text="Select File", command=self.topo)
        self.selectX_button = Button(master, text="Select File", command=self.patch)
        self.selectD_button = Button(master, text="Select Directory", command=self.output)
        self.selectN_button = Button(master, text="Select File", command=self.ns2)
        C1 = Checkbutton(master, text="Activate PCAPs?", variable=pcapValue, onvalue="true", offvalue="false")
        C2 = Checkbutton(master, text="Activate RNG?", variable=rngValue, onvalue="true", offvalue="false")
        self.entry1 = Entry(master, textvariable=self.durationValue, width=12)
        self.entry2 = Entry(master, textvariable=self.ap, width=12)
        self.entry3 = Entry(master, textvariable=self.infra, width=12)
        self.selectR_button = Button(master, text="Run", command=self.runWaf)
        self.close_button = Button(master, text="Close", command=master.quit)

        print "pcap active? " + pcapValue.get()
        print "rng active? " + rngValue.get()

        self.selectT_button.grid(row=0, column=1, sticky=W)
        self.selectX_button.grid(row=1, column=1, sticky=W)
        self.selectD_button.grid(row=2, column=1, sticky=W)
        self.selectN_button.grid(row=3, column=1, sticky=W)
        C1.grid(row=4,column=0, sticky=W)
        C2.grid(row=4,column=1, sticky=W)
        self.entry1.grid(row=6, column=0, sticky=W)
        self.entry2.grid(row=6, column=1, sticky=W)
        self.entry3.grid(row=6, column=2, sticky=W)
        self.selectR_button.grid(row=7, column=0, sticky=W)
        self.close_button.grid(row=7, column=2, sticky=W)


    def runWaf(self):
        if self.topoPath == "":
            print self.topoPath + "\n\n\n"
            tkMessageBox.showerror("Error","You must select a topology file before running.")
        else:
            os.system(self.addOptions(self.topoPath, self.outputPath, self.ns2Path, self.entry1.get(), self.patchPath, pcapValue.get(), self.entry2.get(), self.entry3.get(), rngValue.get()))
            self.master.quit()

    def output(self):
        currdir = os.getcwd()
        tempdir = tkFileDialog.askdirectory(parent=root, initialdir=currdir, title='Please select a directory.')
        if len(tempdir) > 0:
            self.outputPath = tempdir
        else:
            self.outputPath = "core2ns3_Logs/"

    def topo(self):
        currdir = os.getcwd()
        filename = tkFileDialog.askopenfilename(parent=root, initialdir=currdir, title='Please select a topology.', filetypes = (("xml files","*.xml"),("all files","*.*")))

        if len(filename) > 0:
            self.topoPath = filename

    def patch(self):
        currdir = os.getcwd()
        filename = tkFileDialog.askopenfilename(parent=root, initialdir=currdir, title='Please select your application patch.', filetypes = (("xml files","*.xml"),("all files","*.*")))
        if len(filename) > 0:
            self.patchPath = filename

    def ns2(self):
        currdir = os.getcwd()
        filename = tkFileDialog.askopenfilename(parent=root, initialdir=currdir, title='Please select your NS2 mobility file.')
        if len(filename) > 0:
            self.ns2Path = filename


root = Tk()
my_gui = MyGUI(root)
root.mainloop()


