<script>
export default {
    props: {
        id: {
            type: String,
            required: true,
        },
        simple: {
            type: Boolean,
            default: false
        },
        unitDisplay: {
            type: String,
            default: 'right'
        },
        title: {
            type: String,
            default: ''
        },
        name: {
            type: String,
            default: 'Input'
        },
        unit: {
            type: String,
            default: ''
        },
        min: {
            type: Number,
            default: 0
        },
        minTitle: {
            type: String,
            default: 'Less',
        },
        max: {
            type: Number,
            default: 1
        },
        maxTitle: {
            type: String,
            default: 'More',
        },
        step: {
            type: Number,
            default: 1,
        },
        modelValue: {
            type: Number,
            default: 0
        },
        displayTransform: {
            type: Function,
            default: null
        }
    },
    emits: ['update:modelValue'],
    computed: {
        valueDisplay: {
            get() {
                let transformedValue = this.displayTransform
                    ? this.displayTransform(this.value)
                    : this.value;

                if (this.unitDisplay === 'left') {
                    return this.unit + ' ' + transformedValue;
                } else if (this.unitDisplay === 'right') {
                    return transformedValue + ' ' + this.unit;
                }

                return transformedValue;
            }
        },
        value: {
            get() {
                return this.modelValue
            },
            set(value) {
                this.$emit('update:modelValue', value);
            }
        }
    }
}
</script>

<template>
    <div class="slider-input">
        <div class="slider-label">
            <label :for="id" :title="title">
                <slot name="label"/>
            </label>
            <span v-if="simple" class="slider-value">{{ valueDisplay }}</span>
        </div>
        <input :id="id" type="range" class="slider" v-model.number="value"
               :min="min" :max="max" :step="step"/>
        <div v-if="!simple" class="slider-range">
            <span>{{ minTitle }}</span>
            <span>{{ valueDisplay }}</span>
            <span>{{ maxTitle }}</span>
        </div>
        <div v-if="$slots.alert" class="alert alert-success">
            <slot name="alert"/>
        </div>
    </div>
</template>

<style scoped>
.slider-input {
    margin-bottom: 0.5rem;
    display: flex;
    flex-direction: column;
}

.slider-input:last-child {
    margin-bottom: 0;
}

.slider-input > .alert {
    display: block;
    font-size: smaller;
}

div.slider-label {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.5rem;
    font-weight: 500;
}

.slider-value {
    background: var(--primary-gradient);
    color: white;
    padding: 0.25rem 0.75rem;
    border-radius: 50px;
    font-size: 0.75rem;
    font-weight: 600;
}

.slider {
    width: 100%;
    height: 8px;
    border-radius: 4px;
    background: #e2e8f0;
    outline: none;
    appearance: none;
    cursor: pointer;
}

.slider::-webkit-slider-thumb {
    appearance: none;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: var(--primary-gradient);
    cursor: pointer;
    box-shadow: var(--shadow-sm);
    transition: all 0.2s ease;
}

.slider::-webkit-slider-thumb:hover {
    transform: scale(1.1);
    box-shadow: var(--shadow-md);
}

.slider::-moz-range-thumb {
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: var(--primary-gradient);
    cursor: pointer;
    border: none;
    box-shadow: var(--shadow-sm);
}

.slider-range {
    display: flex;
    justify-content: space-between;
    font-size: 0.7rem;
    color: var(--text-secondary);
    margin-top: 0.75rem;
}

.slider-range > span {
    padding: 0.25rem;
    min-width: 100px;
}

.slider-range > span:nth-child(2) {
    background: var(--primary-gradient);
    color: white;
    min-width: auto;
    padding-left: 0.75rem;
    padding-right: 0.75rem;
    border-radius: 50px;
    font-weight: 600;
}

.slider-range > span:last-child {
    text-align: right;
}
</style>