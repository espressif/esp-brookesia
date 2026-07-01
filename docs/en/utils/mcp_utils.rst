.. _utils-mcp-utils-sec-00:

MCP Utils
=========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_mcp_utils`` bridges Brookesia service functions and custom callbacks into ESP MCP tools.

.. rubric:: Core Responsibilities

- Registers service functions as callable MCP tools.
- Builds tool descriptors and handlers from service schema metadata.
- Lets AI agents invoke device capabilities through MCP-compatible tool calls.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

