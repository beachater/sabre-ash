import csv
import matplotlib.pyplot as plt
import itertools
import numpy as np
def plot_cdfs(cdf_csv_files, labels, output_plot):
    if len(cdf_csv_files) != len(labels):
        raise ValueError("The number of CSV files must match the number of labels")

    colors = itertools.cycle(plt.cm.tab10.colors)  # Use a color cycle for different colors

    all_x = []

    for cdf_csv, label in zip(cdf_csv_files, labels):
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
        mean = np.mean(x)

        # Plotting the CDF
        plt.plot(x, y, label=f"{label} (M={mean:.2f})", color=next(colors))
    
    # Determine the overall min and max x values
    min_x = min(all_x)
    max_x = max(all_x)

    plt.xlabel('average bitrate (kbps)')
    # plt.ylabel('CDF')
    # plt.title('Startup Reaction Time')
    plt.legend()
    
    # Set x-ticks to increments of 10 and cover the full range
    plt.xticks(range(0, int(max_x) + 1000, 1000))
    plt.xlim(min_x, max_x)
    
    plt.grid(True)
    plt.savefig(output_plot)
    plt.show()

def main():
    cdf_csv_files = [
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/cdf_bola_data/cdf_time_average_played_bitrate.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/cdf_ashbola_data/cdf_time_average_played_bitrate.csv',
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/cdf_dynamic_data/cdf_time_average_played_bitrate.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/cdf_bolae_data/cdf_time_average_played_bitrate.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/cdf_ashbolaE_data/cdf_time_average_played_bitrate.csv', 
        '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/cdf_data/SD_cdf_NEWashbolaE_data/cdf_time_average_played_bitrate.csv', 
    ]  # List of paths to CDF CSV files

    labels = [
        'Bola',
        'Ashbola',
        'Dynamic',
        'Bola-E',
        'AshBola-E',
        'NEW-ashbolae'
    ]  # Labels for each plot line

    output_plot = '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/plotting/SD_average_bitrate_cdf_plot.png'  # Path to save the plot

    plot_cdfs(cdf_csv_files, labels, output_plot)

if __name__ == '__main__':
    main()
