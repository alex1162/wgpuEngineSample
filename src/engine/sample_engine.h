#pragma once

#include "engine/engine.h"

class SampleEngine : public Engine {

public:
    int frame_counter = 0;
    int total_frames;
    int seq = 0;

	int initialize(Renderer* renderer, sEngineConfiguration configuration = {}) override;
    int post_initialize() override;
    void clean() override;

	void update(float delta_time) override;
	void render() override;
};
