from __future__ import print_function
from __future__ import division
import csv
import matplotlib.pyplot as plt
import numpy as np


tA = []
tB = []
tC = []
tD = []
dataA = []
dataB = []  
dataC = []
dataD = []

with open('sigA.csv') as f:
    reader = csv.reader(f)
    for row in reader:
        tA.append(float(row[0]))
        dataA.append(float(row[1]))

with open('sigB.csv') as f:
    reader = csv.reader(f)
    for row in reader:
        tB.append(float(row[0]))
        dataB.append(float(row[1]))

with open('sigC.csv') as f:
    reader = csv.reader(f)
    for row in reader:
        tC.append(float(row[0]))
        dataC.append(float(row[1]))

with open('sigD.csv') as f:
    reader = csv.reader(f)
    for row in reader:
        tD.append(float(row[0]))
        dataD.append(float(row[1]))

def take_fft(t, data):
    dt = (t[1] - t[0])/len(t)
    Fs = 1.0/dt # Sample rate.
    Ts = t[-1]
    ts = np.arange(0, t[-1], Ts)
    y = data
    n = len(y)
    k = np.arange(n)
    T = n/Fs
    frq = k/T 
    frq = frq[range(int(n/2))] 
    Y = np.fft.fft(y)/n
    Y = Y[range(int(n/2))]

    return frq, abs(Y)

def fir(t, d, data, fL, N, filter, bL):
    dt = (t[1] - t[0])/len(t)
    fS = 1.0/dt # Sample rate.

    # Compute sinc filter.
    h = np.sinc(2 * fL / fS * (np.arange(N) - (N - 1) / 2))

    # Apply window.
    if filter == "blackman":
        h *= np.blackman(N)
    elif filter == "hamming":
        h *= np.hamming(N)
    else:
        pass # Rectangular window.
    

    # Normalize to get unity gain.
    h /= np.sum(h)

    num_taps = len(h)
    filtered_data = np.convolve(d, h, mode='same')
    t_filtered = t

    plot_fft(t, d, data, ("FIR_N=" + str(num_taps) + "_filter=" + filter + "_fL=" + str(fL) + "_bL=" + str(bL)), t_filtered, filtered_data, "C:/Users/aclea/OneDrive/Desktop/Northwestern/me_433/HW10/FIR_images/") 

def plot_fft(t1, data1, data, title, t2=None, data2=None, folder=None):
    frq1, Y1 = take_fft(t1, data1)
    frq2, Y2 = take_fft(t2, data2)

    fig, (ax1, ax2) = plt.subplots(2, 1)
    ax1.plot(t1, data1, 'b', label='Original')
    if t2 is not None and data2 is not None:
        ax1.plot(t2, data2, 'r', label='Filtered')
    ax1.set_xlabel('Time')
    ax1.set_ylabel('Amplitude')
    ax1.legend()
    ax2.loglog(frq1, Y1, 'b', label='Original FFT')
    if t2 is not None and data2 is not None:
        ax2.loglog(frq2, Y2, 'r', label='Filtered FFT')
    ax2.set_xlabel('Freq (Hz)')
    ax2.set_ylabel('|Y(freq)|')
    ax2.legend()
    ax1.set_title("CSV file: " + data + ", " + title)
    plt.draw()
    fig_title = data + "_" + title + ".png"
    plt.savefig(folder + fig_title) # Save the figure as a PNG file
    plt.close(fig) # Close the figure to free up memory
    
def maf(mafnum, t, d, data):
    mafA = []
    mafTA = []
    avg = 0
    for i in range(mafnum, len(d)):
        avg = sum(d[i-mafnum:i]) / mafnum
        mafA.append(avg)
        mafTA.append(t[i])
    plot_fft(t, d, data, ("MAF_" + str(mafnum)), mafTA, mafA, "C:/Users/aclea/OneDrive/Desktop/Northwestern/me_433/HW10/MAF_images/") # **Compares MAF to OG, plot on top.

def iir(A, B, t, d, data):
    iirA = []
    iirTA = []
    avg = 0
    for i in range(1, len(t)):
        iirTA.append(t[i])
        avg = avg*A + d[i]*B
        iirA.append(avg)
    plot_fft(t, d, data, ("IIR_A=" + str(A) + "B=" + str(B)), iirTA, iirA, "C:/Users/aclea/OneDrive/Desktop/Northwestern/me_433/HW10/IIR_images/") 

if __name__ == "__main__":
    # Moving Average Filter
    maf(250, tA, dataA, "sigA")
    maf(400, tB, dataB, "sigB")
    maf(200, tC, dataC, "sigC")
    maf(100, tD, dataD, "sigD")

    # IIR Filter
    iir(0.995, 0.005, tA, dataA, "sigA")
    iir(0.995, 0.005, tB, dataB, "sigB")
    iir(0.995, 0.005, tC, dataC, "sigC")
    iir(0.987, 0.013, tD, dataD, "sigD")

    # FIR Filter
    fir(tA, dataA, "sigA", 999999.67, 200, "hamming", 9999999.92)
    fir(tB, dataB, "sigB", 100000, 800, "blackman", 9999999.92)
    fir(tC, dataC, "sigC", 1000000, 20, "hamming", 999999)
    fir(tD, dataD, "sigD", 10000, 800, "blackman", 9999999.92)