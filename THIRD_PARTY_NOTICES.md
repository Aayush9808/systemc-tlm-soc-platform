# Third-Party Notices

This repository contains original project code plus dependencies used to build or run the generator/model. Third-party dependencies are not relicensed by this repository's MIT license.

## 1. SystemC 3.0.2

- **Project:** SystemC Reference Implementation
- **Version used by this project:** 3.0.2
- **License:** Apache License 2.0
- **Use in this project:** SystemC simulation kernel, modules, processes, events, signals and TLM-2.0 support.
- **Repository:** https://github.com/accellera-official/systemc

SystemC is an external dependency. It is not vendored or copied into this repository. The upstream SystemC project identifies its reference implementation as Apache-2.0 licensed and provides separate LICENSE and NOTICE files.

## 2. PyYAML 6.0.3

- **Project:** PyYAML
- **Version used by this project:** 6.0.3
- **License:** MIT
- **Use in this project:** Parsing and validating YAML SoC/register specifications used by the code generator.
- **Repository:** https://github.com/yaml/pyyaml

PyYAML is installed as a Python dependency from `requirements.txt`; its source code is not vendored into this repository.

## 3. OpenTitan documentation/reference

The assignment asks that peripheral register maps be based on public OpenTitan documentation where practical. This project uses that documentation as a reference for the relevant register-map concepts/subsets where applicable.

- No OpenTitan source code is bundled in this repository.
- The project implements its own reduced SystemC/TLM behavioral models.
- This project does not claim to implement the complete OpenTitan peripheral specifications.

## Project licensing

The original source code and documentation in this repository are released under the MIT License. That project license does not replace or modify the licenses of third-party dependencies listed above.
