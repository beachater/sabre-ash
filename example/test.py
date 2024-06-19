import os
import subprocess
import csv
import math

def cdf(l, margin=0.025):
    l = sorted(l)
    range_val = l[-1] - l[0]
    if range_val > 0:
        margin *= range_val
    inc = 1 / len(l)
    c = []
    y = 0
    if range_val == 0:
        c += [[l[0] - margin, y]]
    for x in l:
        c += [[x, y]]
        y += inc
        c += [[x, y]]
    if range_val == 0:
        c += [[l[-1] + margin, y]]
    return c

def mean_stddev(l):
    mean = sum(l) / len(l)
    var = sum([(x - mean) * (x - mean) for x in l]) / len(l)
    stddev = math.sqrt(var)
    return (mean, stddev)

def run_sabre(trace_file, args):
    command = ['python3', '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/sabre-mmsys18.py', '-n', trace_file] + args
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    results = {}
    for line in completed.stdout.decode('utf-8').split('\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            try:
                results[key.strip()] = float(value.strip())
            except ValueError:
                continue
    if not results:
        raise ValueError(f"No valid results parsed from {trace_file}")
    return results

def process_trace_files(trace_dir, args):
    trace_files = [os.path.join(trace_dir, f) for f in os.listdir(trace_dir) if f.endswith('.json')]
    all_results = []

    count = 0
    for trace_file in trace_files:
        print(f'Processing {trace_file}...')
        try:
            results = run_sabre(trace_file, args)
            results['trace_file'] = os.path.basename(trace_file)  # Store only the file name
            all_results.append(results)
            count += 1
            if (count == 100): break
        except Exception as e:
            print(f"Error processing {trace_file}: {e}")
            print(count)
    
    if not all_results:
        raise ValueError("No results to write to CSV")
    
    return all_results

def save_results_to_csv(results, output_csv):
    keys = sorted(results[0].keys())
    with open(output_csv, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=keys)
        writer.writeheader()
        for result in results:
            writer.writerow(result)

def save_cdfs_to_csv(results, metrics, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    for metric in metrics:
        values = [result.get(metric, 0) for result in results]
        cdf_data = cdf(values)
        cdf_output_csv = os.path.join(output_dir, f'cdf_{metric.replace(" ", "_")}.csv')
        with open(cdf_output_csv, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(['value', 'cdf'])
            for point in cdf_data:
                writer.writerow(point)

def main():
    trace_dir = '//home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/sd_fs'  # directory containing the network trace JSON files
    output_csv = '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/SD_NEWashbolaE_result.csv'
    cdf_output_dir = '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/SD_cdf_NEWashbolaE_data'
    args = ['-ao', '-a', 'ashbolae', '-ab', '-m', '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/bbb.json', '-b', '25']  # Add any additional arguments required by sabre-mmsys18.py

    results = process_trace_files(trace_dir, args)
    save_results_to_csv(results, output_csv)

    metrics = [
        'total played utility',
        'time average played utility',
        'total played bitrate',
        'time average played bitrate',
        'total play time',
        'total rebuffer',
        'rebuffer ratio',
        'time average rebuffer',
        'total rebuffer events',
        'time average rebuffer events',
        'total bitrate change',
        'time average bitrate change',
        'total log bitrate change',
        'time average log bitrate change',
        'time average score',
        'over estimate count',
        'over estimate',
        'leq estimate count',
        'leq estimate',
        'estimate',
        'rampup time',
        'total reaction time'
    ]

    save_cdfs_to_csv(results, metrics, cdf_output_dir)

if __name__ == '__main__':
    main()
