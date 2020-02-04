# Python Examples
This directory contains Python based examples.
Some of these examples require additional Python modules or have been optimized to work with Jupyter Notebooks.

# Local Python Setup
Create an virtual environment, activate the `venv` and install required dependencies.
```
python3 -m venv venv
source venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install paho-mqtt
python3 -m pip install pandas
python3 -m pip install openpyxl
```

### Prepare Jupyter Lab Environment
Install required packages to execute Jupyter Lab.
```
python3 -m pip install nodejs
python3 -m pip install npm
python3 -m pip install jupyterlab
python3 -m pip install bokeh
python3 -m pip install ipywidgets

jupyter labextension install jupyterlab_bokeh
jupyter labextension install @jupyterlab/celltags
jupyter labextension install @jupyter-widgets/jupyterlab-manager
```

Run the Jupyter Lab instance.
```
jupyter lab
```