# modelhandler.py
import sys
import json
import numpy as np
import tensorflow as tf

def load_model(model_path):
    custom_objects = {'mse': tf.keras.losses.MeanSquaredError()}
    return tf.keras.models.load_model(model_path, custom_objects=custom_objects)

def predict(model, data):
    prediction = model.predict(data)
    return prediction[0][0]  # Ensure this outputs a single float value.

if __name__ == '__main__':
    debug = 'debug' in sys.argv  # Check if 'debug' is a command line argument

    input_json = sys.stdin.read()
    input_data = json.loads(input_json)

    model_path = '/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/src/ashBOLA3g4g.h5'
    model = load_model(model_path)
    data = np.array(input_data['data']).reshape((1, 10, 3))

    output = predict(model, data)
    
    if debug:
        print("Debug information: Model output", output)
        
    print(output)  # Print the numeric result to stdout.
