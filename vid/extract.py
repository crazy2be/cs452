import numpy
import cv2
import scipy.ndimage
#from matplotlib import pyplot, cm
import sys
import itertools

cap = cv2.VideoCapture(sys.argv[1])

start = 1750
end = -1

def capture_iterator(cap, start, end):
    # drop the first `start` frames
    for i in range(start):
        if not cap.grab():
            return

    count = 0

    while (end < 0 or count <= end - start):
        ret, frame = cap.read()
        if not ret:
            return
        yield frame


def extract_centroid(frame):
    filtered = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    greenish = numpy.array((129.0, 182.0, 120.0))
    a = filtered[:,:] - greenish

    tolerance = 30
    filtered[numpy.sqrt((a*a).sum(axis=2)) > tolerance] = (0, 0, 0)

    centroid = scipy.ndimage.measurements.center_of_mass(filtered)

    return (centroid[0], centroid[1])

for i, centroid in enumerate(itertools.imap(extract_centroid, capture_iterator(cap, start, end))):
    print i, centroid[0], centroid[1]

#print centroids
