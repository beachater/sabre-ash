import csv
import matplotlib.pyplot as plt
import itertools
import numpy as np


def plot_cdfs(cdf_csv_files, labels, output_plot):
    if len(cdf_csv_files) != len(labels):
        raise ValueError("The number of CSV files must match the number of labels")

    colors = itertools.cycle(plt.cm.tab10.colors)  # Use a color cycle for different colors
    markers = itertools.cycle(['x', 'd', 'o', '.', '+'])  # Cycle through different markers

    all_x = []
    all_y = []

    for cdf_csv, label, marker in zip(cdf_csv_files, labels, markers):
        x = []
        y = []
        
        # Read data from the CSV file
        with open(cdf_csv, 'r') as csvfile:
            reader = csv.reader(csvfile)
            next(reader)  # skip header
            for row in reader:
                x.append(float(row[0]))
                y.append(float(row[1]))
        
        all_x.extend(x)
        all_y.extend(y)
        
        mean = np.mean(x)
        print(label, ":", mean)
        

        mark = 0
        # Plotting the CDF with markers
        plt.plot(x, y, label=f"{label}", color=next(colors), marker=marker[mark], markevery=15)
        mark +=1
    
    # Determine the overall min and max x values
    min_x = min(all_x)
    max_x = max(all_x)
    max_y = max(all_y)
    print(max_x)

    plt.xlabel('rebuffer ratio')
    # plt.ylabel('CDF')
    # plt.title('Startup Reaction Time')
    plt.legend()
    
    # Set x-ticks to increments of 10 and cover the full range
    plt.xticks(np.arange(min_x, 0.5, 0.1))
    # plt.yticks(range(int(min_x), int(max_y)))
    plt.xlim(min_x, max_x)
    
    plt.grid(True)
    plt.savefig(output_plot)
    plt.show()

def main():
    cdf_csv_files = [
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/HDcdf_bola_data/cdf_rebuffer_ratio.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/HDcdf_ashbola_data/cdf_rebuffer_ratio.csv',
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/HDcdf_dynamic_data/cdf_rebuffer_ratio.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/HDcdf_bolaE_data/cdf_rebuffer_ratio.csv', 
        # '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/SD_cdf_NEWashbolaE_data/cdf_rebuffer_ratio.csv', 
        
    ]  # List of paths to CDF CSV files

    labels = [
        'Bola',
        'Ashbola',
        'Dynamic',
        'Bola-E',
        # 'AshBola-E'
    ]  # Labels for each plot line

    output_plot = '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/HD_rebuffer_cdf_plot.png'  # Path to save the plot

    plot_cdfs(cdf_csv_files, labels, output_plot)

if __name__ == '__main__':
    main()
