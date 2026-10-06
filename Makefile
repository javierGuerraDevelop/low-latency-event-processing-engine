.PHONY: build test format deploy

build:
	cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build build -j"$$(nproc)"

test: build
	ctest --test-dir build --output-on-failure

format:
	clang-format -i src/*.cpp include/*.h tests/*.cpp

deploy:
	@echo "Enter commit message: " && read commitMessage && \
	git add . && \
	git commit -m "$$commitMessage" && \
	git push
