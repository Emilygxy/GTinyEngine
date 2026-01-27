#pragma once

#include "framework/RenderGraph.h"
#include <string>
#include <fstream>
#include <sstream>

namespace te
{
    // RenderGraph Visualizer
    // Generates .dot files for Graphviz visualization
    class RenderGraphVisualizer
    {
    public:
        RenderGraphVisualizer() = default;
        ~RenderGraphVisualizer() = default;

        // Generate .dot file from compiled graph
        bool GenerateDotFile(const CompiledGraph& graph, const std::string& filename);

        // Generate .dot content as string
        std::string GenerateDotContent(const CompiledGraph& graph);

    private:
        // Helper methods for generating different parts of the graph
        void GenerateGraphHeader(std::ostringstream& oss);
        void GenerateGraphFooter(std::ostringstream& oss);
        
        void GeneratePassNodes(const CompiledGraph& graph, std::ostringstream& oss);
        void GenerateResourceNodes(const CompiledGraph& graph, std::ostringstream& oss);
        void GenerateDependencyEdges(const CompiledGraph& graph, std::ostringstream& oss);
        void GenerateResourceEdges(const CompiledGraph& graph, std::ostringstream& oss);
        void GenerateAliasEdges(const CompiledGraph& graph, std::ostringstream& oss);
        
        // Helper to sanitize node names for dot format
        std::string SanitizeNodeName(const std::string& name);
        
        // Helper to get resource format string
        std::string GetResourceFormatString(RenderTargetFormat format);
        
        // Helper to get pass type color
        std::string GetPassTypeColor(RenderPassType type);
    };
}
