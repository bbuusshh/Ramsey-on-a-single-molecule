import numpy as np

def assign_trigs_to_clicks(data, channels=[1,2,3,4],trig=None,trig_ch=5, pix_trigs=None):
    """Assigns trigger times to the corresponding clicks (replaces the field of overflow with time_from_trigger for each click).
    -- takes memmaped data returned by the get_dump(fname) function
    RETURNS: timetags data with times from trigger field:
    For example:
    time_from_trig	channel	      time
        3989	       4	 636522181766531
        77348	       2	 636522192370617
        29343	       4	 636522234085002
    Example for plotting the result:
    --Histogram:
    y, x = np.histogram(data['time_from_trig'][(data['channel']==1)], bins=1000, range=(0, trig))
    plt.plot(x[:-1], y)
    """
    assert np.isin(channels, np.unique(data['channel'])).all(), "Some of the channels aren't present in the tagstream file."

    assert ((len(list(data.dtype.fields))) != 3 & np.isin(['channel', 'time'], list(data.dtype.fields), invert=True).all()), "The dtype should be [('smth..', 'channel', 'time')']"
    if 'time_from_trig' not in list(data.dtype.fields):
        DUMP_TAGTYPE = np.dtype([('time_from_trig', np.dtype('<u4')),
                            ('channel', np.dtype('<i4')),
                            ('time', np.dtype('<u8'))])
        data=np.array(data, dtype=DUMP_TAGTYPE)
    if trig == None:
        trig=get_trig_len(data, trig_ch=trig_ch)
    i=1
    if pix_trigs != None:
        # skip pixel triggers
        data['time_from_trig'][np.isin(data['channel'],pix_trigs)] = 1
    while data[(data['time_from_trig'] == 0) & (data['channel'] != trig_ch)].shape[0] != 0:
        data_shifted = np.roll(data, -i)
        delta = (data_shifted['time'] - data['time'])[(data['time_from_trig'] == 0) & (data_shifted['channel'] == trig_ch) & (np.isin(data['channel'], channels))]
        data['time_from_trig'][(data['time_from_trig'] == 0) & (data_shifted['channel'] == trig_ch) & (np.isin(data['channel'], channels))] \
    = trig - np.mod(delta, trig)
        i+=1
        if i > 50:
            break
    return data

def get_trig_len(data, trig_ch = 5):
    """
    Return the length of the trigger.
    """
    tr = np.diff(data['time'][data['channel'] == trig_ch])
    return tr[tr//np.min(tr) == 1].mean()



    [data['channel'] != trig_ch]

def shift_channels(data, shift, channels):
    '''
        Shift specified channels
    '''
    if not data.flags['WRITEABLE']:
        data=data.copy()
    data['time'][np.isin(data['channel'], channels)] = data['time'][np.isin(data['channel'], channels)] - shift
    data = data[np.argsort(data['time'])]
    return data

def correlation(data, corr_window=300_000, start_chs=[1], stop_chs=[4], trig_ch=5):
    """
    Build g^2 and k-photon probability function from Time Tagger data formated as :
    DUMP_TAGTYPE = np.dtype([('smth...', np.dtype('<u4')),
                                 ('channel', np.dtype('<i4')),
                                 ('time', np.dtype('<u8'))])
    returns autocorr_function and a list of k-photon probability data points
    Example for plotting the result:
    --Autocorrelation:
    autoc_diffs_tot, autoc_diffs = correlation(data)
    y, x = np.histogram(autoc_diffs_tot, bins=1000, range=(-10*trig, 10*trig))
    plt.plot(x[:-1], y)
    -- k-photon probability:
    corr_window=100_000_000
    autoc_diffs_tot, autoc_diffs = correlation(data, corr_window=corr_window)
    for k in autoc_diffs[::2]:
        y, x = np.histogram(k, bins=1000, range=(0, corr_window))
        plt.plot(x[:-1], y)
    """
    autoc_data = data[data['channel'] != 5]
    autoc_diffs = []
    shift=0
    while True:#shift != 20:
        shift += 1
        diffs = autoc_data['time'] - np.roll(autoc_data['time'], shift)
        dif_ch = np.roll(autoc_data['channel'], shift)
        diffs_1 = diffs[(diffs < corr_window) & np.isin(autoc_data['channel'], start_chs) & np.isin(dif_ch, stop_chs)]
        autoc_diffs.append(diffs_1.astype('int32'))
        diffs_2 = diffs[(diffs < corr_window) & np.isin(autoc_data['channel'], stop_chs) & np.isin(dif_ch, start_chs)]
        autoc_diffs.append(-diffs_2.astype('int32'))
        if (len(diffs_1) == 0) and (len(diffs_2) == 0):
            break
    autoc_diffs = np.array(autoc_diffs)
    autoc_diffs_tot = np.concatenate(autoc_diffs)
    return autoc_diffs_tot, autoc_diffs
