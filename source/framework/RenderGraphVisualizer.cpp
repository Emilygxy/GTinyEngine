#include "framework/RenderGraphVisualizer.h"
#include "framework/RenderGraph.h"
#include "framework/RenderPass.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

namespace te
{
    bool RenderGraphVisualizer::GenerateDotFile(const CompiledGraph& graph, const std::string& filename)
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cout << "RenderGraphVisualizer::GenerateDotFile: Failed to open file " << filename << std::endl;
            return false;
        }

        std::string content = GenerateDotContent(graph);
        file << content;
        file.close();

        std::cout << "RenderGraphVisualizer: Generated .dot file: " << filename << std::endl;
        return true;
    }

    std::string RenderGraphVisualizer::GenerateDotContent(const CompiledGraph& graph)
    {
        std::ostringstream oss;
        
        GenerateGraphHeader(oss);
        
        // Generate pass nodes (inside cluster_passes)
        GeneratePassNodes(graph, oss);
        
        // Close cluster_passes
        oss << "    }\n\n";
        
        // Generate resource nodes (inside cluster_resources)
        oss << "    subgraph cluster_resources {\n";
        oss << "        label=\"Resources\";\n";
        oss << "        style=filled;\n";
        oss << "        color=lightgreen;\n";
        oss << "        fillcolor=\"#E6FFE6\";\n";
        GenerateResourceNodes(graph, oss);
        oss << "    }\n\n";
        
        // Generate edges (outside clusters)
        GenerateDependencyEdges(graph, oss);
        GenerateResourceEdges(graph, oss);
        GenerateAliasEdges(graph, oss);
        
        GenerateGraphFooter(oss);
        
        return oss.str();
    }

    void RenderGraphVisualizer::GenerateGraphHeader(std::ostringstream& oss)
    {
        oss << "digraph RenderGraph {\n";
        oss << "    rankdir=TB;\n";
        oss << "    node [shape=box, style=rounded];\n";
        oss << "    edge [fontsize=10];\n\n";
        
        // Define subgraph cluster for passes
        oss << "    subgraph cluster_passes {\n";
        oss << "        label=\"Render Passes\";\n";
        oss << "        style=filled;\n";
        oss << "        color=lightblue;\n";
        oss << "        fillcolor=\"#E6F3FF\";\n";
    }

    void RenderGraphVisualizer::GenerateGraphFooter(std::ostringstream& oss)
    {
        oss << "}\n";
    }

    void RenderGraphVisualizer::GeneratePassNodes(const CompiledGraph& graph, std::ostringstream& oss)
    {
        for (size_t i = 0; i < graph.passes.size(); ++i)
        {
            const auto& pass = graph.passes[i];
            std::string nodeName = SanitizeNodeName("pass_" + pass.name);
            
            // Build label with pass information
            std::ostringstream label;
            label << pass.name;
            
            if (pass.pass)
            {
                const auto& config = pass.pass->GetConfig();
                
                // Add pass type name
                switch (config.type)
                {
                case RenderPassType::Geometry: label << "\\n[Geometry]"; break;
                case RenderPassType::Background: label << "\\n[Background]"; break;
                case RenderPassType::Skybox: label << "\\n[Skybox]"; break;
                case RenderPassType::Base: label << "\\n[Base]"; break;
                case RenderPassType::PostProcess: label << "\\n[PostProcess]"; break;
                case RenderPassType::Shadow: label << "\\n[Shadow]"; break;
                case RenderPassType::UI: label << "\\n[UI]"; break;
                case RenderPassType::Custom: label << "\\n[Custom]"; break;
                default: break;
                }
                
                if (!pass.reads.empty() || !pass.writes.empty())
                {
                    label << "\\nR:" << pass.reads.size() << " W:" << pass.writes.size();
                }
            }
            
            // Get color based on pass type
            std::string color = "\"#ADD8E6\"";  // lightblue
            if (pass.pass)
            {
                color = GetPassTypeColor(pass.pass->GetConfig().type);
            }
            
            oss << "        " << nodeName << " [label=\"" << label.str() 
                << "\", fillcolor=" << color << ", style=\"rounded,filled\"];\n";
        }
    }

    void RenderGraphVisualizer::GenerateResourceNodes(const CompiledGraph& graph, std::ostringstream& oss)
    {
        for (const auto& [name, desc] : graph.resources)
        {
            std::string nodeName = SanitizeNodeName("res_" + name);
            
            // Build label with resource information
            std::ostringstream label;
            label << name;
            label << "\\n" << GetResourceFormatString(desc.format);
            label << "\\n" << desc.width << "x" << desc.height;
            
            if (desc.allowAliasing)
            {
                label << "\\n[Alias]";
            }
            
            // Check if resource is aliased
            std::string color = "\"#90EE90\"";  // lightgreen
            auto aliasIt = graph.aliases.find(name);
            if (aliasIt != graph.aliases.end())
            {
                color = "\"#FFFFE0\"";  // lightyellow
                label << "\\n-> " << aliasIt->second;
            }
            
            // Check lifetime
            auto lifetimeIt = graph.lifetimes.find(name);
            if (lifetimeIt != graph.lifetimes.end())
            {
                const auto& lifetime = lifetimeIt->second;
                label << "\\nLife: " << lifetime.firstUse << "-" << lifetime.lastUse;
            }
            
            oss << "        " << nodeName << " [label=\"" << label.str() 
                << "\", fillcolor=" << color << ", style=\"rounded,filled\", shape=ellipse];\n";
        }
    }

    void RenderGraphVisualizer::GenerateDependencyEdges(const CompiledGraph& graph, std::ostringstream& oss)
    {
        oss << "\n    // Pass dependency edges\n";
        
        // Build pass name to index map
        std::unordered_map<std::string, size_t> passNameToIndex;
        for (size_t i = 0; i < graph.passes.size(); ++i)
        {
            passNameToIndex[graph.passes[i].name] = i;
        }
        
        for (size_t i = 0; i < graph.passes.size(); ++i)
        {
            const auto& pass = graph.passes[i];
            std::string fromNode = SanitizeNodeName("pass_" + pass.name);
            
            for (const auto& depName : pass.dependencies)
            {
                auto it = passNameToIndex.find(depName);
                if (it != passNameToIndex.end())
                {
                    std::string toNode = SanitizeNodeName("pass_" + graph.passes[it->second].name);
                    oss << "    " << toNode << " -> " << fromNode 
                        << " [color=blue, style=solid, label=\"depends\", penwidth=2];\n";
                }
            }
        }
    }

    void RenderGraphVisualizer::GenerateResourceEdges(const CompiledGraph& graph, std::ostringstream& oss)
    {
        oss << "\n    // Resource usage edges\n";
        
        for (size_t i = 0; i < graph.passes.size(); ++i)
        {
            const auto& pass = graph.passes[i];
            std::string passNode = SanitizeNodeName("pass_" + pass.name);
            
            // Read edges (resource -> pass, green)
            for (const auto& read : pass.reads)
            {
                std::string resNode = SanitizeNodeName("res_" + read.resourceName);
                oss << "    " << resNode << " -> " << passNode 
                    << " [color=green, style=solid, label=\"read\"];\n";
            }
            
            // Write edges (pass -> resource, red)
            for (const auto& write : pass.writes)
            {
                std::string resNode = SanitizeNodeName("res_" + write.resourceName);
                oss << "    " << passNode << " -> " << resNode 
                    << " [color=red, style=solid, label=\"write\"];\n";
            }
        }
    }

    void RenderGraphVisualizer::GenerateAliasEdges(const CompiledGraph& graph, std::ostringstream& oss)
    {
        if (graph.aliases.empty())
            return;
            
        oss << "\n    // Resource alias edges\n";
        
        for (const auto& [aliasName, actualName] : graph.aliases)
        {
            std::string aliasNode = SanitizeNodeName("res_" + aliasName);
            std::string actualNode = SanitizeNodeName("res_" + actualName);
            
            oss << "    " << aliasNode << " -> " << actualNode 
                << " [color=orange, style=dashed, label=\"alias\", penwidth=2];\n";
        }
    }

    std::string RenderGraphVisualizer::SanitizeNodeName(const std::string& name)
    {
        std::string sanitized = name;
        
        // Replace spaces and special characters with underscores
        std::replace(sanitized.begin(), sanitized.end(), ' ', '_');
        std::replace(sanitized.begin(), sanitized.end(), '-', '_');
        std::replace(sanitized.begin(), sanitized.end(), '.', '_');
        std::replace(sanitized.begin(), sanitized.end(), ':', '_');
        
        return sanitized;
    }

    std::string RenderGraphVisualizer::GetResourceFormatString(RenderTargetFormat format)
    {
        switch (format)
        {
        case RenderTargetFormat::RGB8: return "RGB8";
        case RenderTargetFormat::RGBA8: return "RGBA8";
        case RenderTargetFormat::RGB16F: return "RGB16F";
        case RenderTargetFormat::RGBA16F: return "RGBA16F";
        case RenderTargetFormat::RGB32F: return "RGB32F";
        case RenderTargetFormat::RGBA32F: return "RGBA32F";
        case RenderTargetFormat::Depth24: return "Depth24";
        case RenderTargetFormat::Depth32F: return "Depth32F";
        case RenderTargetFormat::Depth24Stencil8: return "Depth24Stencil8";
        default: return "Unknown";
        }
    }

    std::string RenderGraphVisualizer::GetPassTypeColor(RenderPassType type)
    {
        switch (type)
        {
        case RenderPassType::Geometry:
            return "\"#ADD8E6\"";  // lightblue
        case RenderPassType::Background:
            return "\"#E0FFFF\"";  // lightcyan
        case RenderPassType::Skybox:
            return "\"#B0C4DE\"";  // lightsteelblue
        case RenderPassType::Base:
            return "\"#90EE90\"";  // lightgreen
        case RenderPassType::PostProcess:
            return "\"#FFB6C1\"";  // lightpink
        case RenderPassType::Shadow:
            return "\"#D3D3D3\"";  // lightgray
        case RenderPassType::UI:
            return "\"#FFFFE0\"";  // lightyellow
        case RenderPassType::Custom:
            return "\"#F08080\"";  // lightcoral
        default:
            return "\"#ADD8E6\"";  // lightblue
        }
    }
}
