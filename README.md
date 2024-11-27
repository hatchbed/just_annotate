# JustAnnotate

A simple video annotation tool for labeling time spans.

![](https://github.com/hatchbed/just_annotate/wiki/screenshot.png)

## Install Build Prerequisites

    $ sudo apt update
    $ sudo apt install build-essential cmake git libgl-dev libglib2.0-dev \
                       libglu1-mesa-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                       libspdlog-dev libssl-dev libxcursor-dev libxi-dev libxinerama-dev \
                       libxrandr-dev nlohmann-json3-dev pkg-config

## Build

    $ git clone https://github.com/hatchbed/just_annotate.git
    $ mkdir -p just_annotate/build
    $ cd just_annotate/build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make -j4 

## Run

    $ ./just_annotate

## File Format

Data is stored in a simple JSON structure containing the annotation classes and file annotations as ranges in seconds.
The names of each file are stored, but for disambiguation, the SHA-256 hash of each file are also stored.

### Example

    {
        "annotation_classes": [
            {
                "color": {
                    "b": 0.0,
                    "g": 0.7019608020782471,
                    "r": 0.9019607901573181
                },
                "id": 0,
                "name": "uncertain"
            },
            {
                "color": {
                    "b": 0.9019607901573181,
                    "g": 0.3176470696926117,
                    "r": 0.0
                },
                "id": 1,
                "name": "open"
            },
            {
                "color": {
                    "b": 0.46666666865348816,
                    "g": 0.0,
                    "r": 0.9019607901573181
                },
                "id": 2,
                "name": "closed"
            }
        ],
        "files": [
            {
                "annotations": [
                    {
                        "id": 0,
                        "spans": [
                            [
                                2.8908002376556396,
                                3.738971471786499
                            ],
                            [
                                42.37736892700195,
                                42.86525344848633
                            ],
                            [
                                50.5208854675293,
                                50.83022689819336
                            ]
                        ]
                    },
                    {
                        "id": 1,
                        "spans": [
                            [
                                0.0,
                                2.8908002376556396
                            ],
                            [
                                42.86525344848633,
                                50.5208854675293
                            ]
                        ]
                    },
                    {
                        "id": 2,
                        "spans": [
                            [
                                3.738971471786499,
                                42.37736892700195
                            ],
                            [
                                50.83022689819336,
                                84.0
                            ]
                        ]
                    }
                ],
                "hash": "01c1b5711f3481c9f57203538bc1137f2aecb72bc6cf81e5f60d8dc4c5732cb7",
                "names": [
                    "video_name.mp4"
                ]
            },
        ]
    }

### Schema

    {
        "$schema": "http://json-schema.org/draft-07/schema#",
        "type": "object",
        "properties": {
            "annotation_classes": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "color": {
                            "type": "object",
                            "properties": {
                                "b": { "type": "number" },
                                "g": { "type": "number" },
                                "r": { "type": "number" }
                            },
                            "required": ["b", "g", "r"]
                        },
                        "id": { "type": "integer" },
                        "name": { "type": "string" }
                    },
                    "required": ["color", "id", "name"]
                }
            },
            "files": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "annotations": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "id": { "type": "integer" },
                                    "spans": {
                                        "type": "array",
                                        "items": {
                                            "type": "array",
                                            "items": { "type": "number" },
                                            "minItems": 2,
                                            "maxItems": 2
                                        }
                                    }
                                },
                                "required": ["id", "spans"]
                            }
                        },
                        "hash": { "type": "string" },
                        "names": {
                            "type": "array",
                            "items": { "type": "string" }
                        }
                    },
                    "required": ["annotations", "hash", "names"]
                }
            }
        },
        "required": ["annotation_classes", "files"]
    }
