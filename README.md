# vision
Datasets, Transforms and Models specific to Computer Vision


## Installation
- install the nightly version of `oneflow`
```bash
python3 -m pip install oneflow -f https://staging.oneflow.info/branch/master/cu102
```
- install latest version of `flowvision`
```bash
pip install flowvision==0.0.3
```

## Supported Model
All of supported models can be found in our model summary page [here](MODEL_SUMMARY.md).


## Usage
<details>
<summary> <b> Quick Start </b> </summary>

- list supported model
```python
from flowvision import ModelCreator
ModelCreator.model_table()
```

- search supported model by wildcard
```python
from flowvision import ModelCreator
ModelCreator.model_table("*vit*", pretrained=True)
ModelCreator.model_table("*vit*", pretrained=False)
ModelCreator.model_table('alexnet')
```

- create model use `ModelCreator`
```python
from flowvision import ModelCreator
model = ModelCreator.create_model('alexnet', pretrained=True)
```

</details>

<details>
<summary> <b> ModelCreator </b> </summary>

- Create model in a simple way
```python
from flowvision.models import ModelCreator
model = ModelCreator.create_model('alexnet', pretrained=True)
```
the pretrained weight will be saved to `./checkpoints`

- Supported model table
```python
from flowvision.models import ModelCreator
ModelCreator.model_table()
```
```
           Models            
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━┓
┃ Name         ┃ Pretrained ┃
┡━━━━━━━━━━━━━━╇━━━━━━━━━━━━┩
│ alexnet      │ true       │
│ vit_b_16_224 │ false      │
│ vit_b_16_384 │ true       │
│ vit_b_32_224 │ false      │
│ vit_b_32_384 │ true       │
│ vit_l_16_384 │ true       │
│ vit_l_32_384 │ true       │
└──────────────┴────────────┘
```
show all of the supported model in the table manner

- List models with pretrained weights
```python
from flowvision.models import ModelCreator
ModelCreator.model_table(pretrained=True)
```
```
           Models            
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━┓
┃ Name         ┃ Pretrained ┃
┡━━━━━━━━━━━━━━╇━━━━━━━━━━━━┩
│ alexnet      │ true       │
│ vit_b_16_384 │ true       │
│ vit_b_32_384 │ true       │
│ vit_l_16_384 │ true       │
│ vit_l_32_384 │ true       │
└──────────────┴────────────┘
```
- Search for model by Wildcard
```python
from flowvision.models import ModelCreator
ModelCreator.model_table('vit*')
```
```
           Models            
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━┓
┃ Name         ┃ Pretrained ┃
┡━━━━━━━━━━━━━━╇━━━━━━━━━━━━┩
│ vit_b_16_224 │ false      │
│ vit_b_16_384 │ true       │
│ vit_b_32_224 │ false      │
│ vit_b_32_384 │ true       │
│ vit_l_16_384 │ true       │
│ vit_l_32_384 │ true       │
└──────────────┴────────────┘
```
- Search for model with pretrained weights by Wildcard
```python
from flowvision.models import ModelCreator
ModelCreator.model_table('vit*', pretrained=True)
```
```
           Models            
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━┓
┃ Name         ┃ Pretrained ┃
┡━━━━━━━━━━━━━━╇━━━━━━━━━━━━┩
│ vit_b_16_384 │ true       │
│ vit_b_32_384 │ true       │
│ vit_l_16_384 │ true       │
│ vit_l_32_384 │ true       │
└──────────────┴────────────┘
```

</details>

## Model Zoo
We did all our tests under the same setting, please check the model page [here](MODEL_ZOO.md) for more details.