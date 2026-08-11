#pragma once
#include "../FragmentTypes.hpp"
#include <fragments/fluted/FlutedGenerator.hpp>
#include <fragments/fo2/FO2Generator.hpp>
#include <string>
#include <vector>


void saveDatasetFile(const std::string& path, const std::string& formula);
std::vector<PredInfo> buildVocab(const std::string& s);

// TEMPLATE BASE DECLARATION
template <typename Gen>
void generateDatasetsNative(Gen& gen, const std::string& fragment, int count);


template <>
void generateDatasetsNative<FlutedGenerator>(FlutedGenerator& gen, const std::string& fragment, int count);

template <>
void generateDatasetsNative<FO2Generator>(FO2Generator& gen, const std::string& fragment, int count);