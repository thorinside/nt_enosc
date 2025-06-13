# PRD: Refactor Data Generation to Use Static Python Script

## 1. Background

The project currently uses `dynamic_data.cc` and `dynamic_data.hh` to generate necessary data tables at runtime. This was an adaptation from the original `enosc` project, which used a Python script (`data_compiler.py`) to generate static `data.cc` and `data.hh` files at build time. To achieve higher fidelity with the original implementation, improve performance by pre-calculating data, and simplify the C++ codebase, we are reverting to the static data generation method.

## 2. Goals

-   Achieve 1:1 data fidelity with the original `enosc` hardware module.
-   Simplify the C++ code by removing runtime data generation logic.
-   Ensure the build process is self-contained and automatically generates the required data files.
-   Refactor the main plugin (`nt_enosc.cpp`) to correctly use the new static data.

## 3. Requirements

### 3.1. Remove Dynamic Data Generation

1.  **Delete Files**: The files `dynamic_data.cc` and `dynamic_data.hh` must be deleted from the project repository.
2.  **Remove Code References**: All `#include` statements and other references to `dynamic_data.cc`, `dynamic_data.hh`, and the `DynamicData` class within the codebase (primarily in `nt_enosc.cpp`) must be removed.

### 3.2. Integrate Static Data Generation Script

1.  **Locate Script**: The Python script responsible for data generation from the original `enosc` source tree must be located and placed in a suitable directory (e.g., `scripts/`).
2.  **Update Makefile**: The project's `Makefile` must be modified to add a new build rule that executes the Python script. This rule should ensure that `data.cc` and `data.hh` are generated before the C++ source code is compiled. The generated files should be placed in a build or output directory that is accessible to the compiler.

### 3.3. Integrate Statically Generated Data

1.  **Include Header**: The main plugin file, `nt_enosc.cpp`, must be updated to include the newly generated `data.hh`.
2.  **Compile Source**: The generated `data.cc` file must be added to the list of source files to be compiled in the `Makefile`.
3.  **Update Data Access**: The logic in `nt_enosc.cpp` must be refactored to use the data structures, variables, and constants provided by `data.hh` and `data.cc`. This includes updating how the `Data::data` global pointer is initialized and used.

### 3.4. Refactor C++ Code

1.  **Modify `calculateStaticRequirements`**: In `nt_enosc.cpp`, the `calculateStaticRequirements` function must be updated. Instead of allocating space for a `DynamicData` object, it should allocate the correct amount of DRAM based on the size of the data structures defined in the generated `data.hh`.
2.  **Modify `initialise`**: The `initialise` function must be updated. The logic for `new (ptrs.dram) DynamicData` should be removed. The global `Data::data` pointer should be set to point to the statically defined data structure from the generated files.

## 4. Acceptance Criteria

-   `dynamic_data.cc` and `dynamic_data.hh` no longer exist in the codebase.
-   Running `make` successfully triggers the Python script, generating `data.cc` and `data.hh`.
-   The project compiles successfully without any errors related to missing data or incorrect types.
-   The `calculateStaticRequirements` function in `nt_enosc.cpp` correctly defines the memory requirements for the static data.
-   The final compiled plugin loads and runs correctly, demonstrating that the static data is being used as intended. 