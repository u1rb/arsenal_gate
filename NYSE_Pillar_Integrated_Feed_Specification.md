# NYSE PILLAR INTEGRATED FEED - CLIENT SPECIFICATION

## NYSE INTEGRATED FEED
## NYSE AMERICAN INTEGRATED FEED
## NYSE ARCA INTEGRATED FEED
## NYSE TEXAS INTEGRATED FEED
## NYSE NATIONAL INTEGRATED FEED

**Version:** 2.5g  
**Date:** October 23rd, 2025

---

©2025 NYSE Group, Inc. All Rights Reserved.

---

## PREFACE

### DOCUMENT HISTORY

| VERSION NO. | DATE | CHANGE DESCRIPTION |
|-------------|------|-------------------|
| 2.2 | 12/01/2018 | • Updates to Auctions and the Imbalances msg for NYSE under Pillar.<br>• Renamed Closing Only Clearing Price to Auction Only Clearing Price.<br><br>The 3 new trailing fields in the Imbalance message contain only default values in non-NYSE markets. |
| 2.2a | 03/08/2019 | • Updates to NYSE Imbalance start time<br>• Defaulted the Significant imbalance field in Msg Type 105 |
| 2.3 | 06/14/2019 | Added support for NYSE Chicago Integrated Feed - product ID, hours of operation, etc… (Go Live 11/2019) |
| 2.3a | 10/25/2019 | Additional clarifications/cleanup post Tape A migration to Pillar.<br>• Corrected 4pm security status message for NYSE.<br>• Clarified FirmID field in the AddOrder message (Msg Type 100)<br>• Clarified Position Change field in the Modify (Msg Type 101)<br>• Clarified DBExecID field in Execution Message (Msg Type 103).<br>• Removed implementation note on RPI message (Msg Type 114).<br>• Clarified field definitions for CBCP, AICP on Imbalance Msg (Msg Type 105)<br>• Corrected Core opening time in Appendix A. |
| 2.3b | 01/14/2021 | Updated that the Exchange would disseminate Auction Imbalance Information if a security is an IPO or Direct Listing. |
| 2.3c | 07/30/2021 | Updated the MOC LOC Cutoff timer to 10 minutes in Section 14.1 |
| 2.3d | 08/12/2021 | Updated the Arca - Order Entry Start Time to 2:30am in Section 1.3 |
| 2.4 | 12/17/2021 | Addition of Trade Conditions 1-4 on msgtype 103 and msgtype 110, in place of DBEXECID - all markets in Q1 2022 |
| 2.4a | 03/04/2022 | Updated ParitySplits fieldname to be 'Reserved' and for future use. Msgtypes 100, 101, 102, 103, 104 and 106.<br><br>Updated IP address sheet link in Reference Materials. |
| 2.4b | 03/21/2022 | Updated NYSE logo and document format. No content changes. |
| 2.5 | 05/16/2022 | Msgtype 101 - Updated Modify message 'Reserved 1' field (byte 33) to 'Side' (ASCII)<br><br>Msgtype 104 - Updated Replace message 'Reserved 1' field (byte 40) to 'Side' (ASCII) |
| 2.5a | 11/30/2022 | Updated Section 14.3 for clarifications on imbalance publication timings. |
| 2.5b | 02/22/2024 | Updated section 1.3.2 Control Message Types and section 14.3 to account for new feed start time of 2:00am ET |
| 2.5c | 07/25/2024 | Updated section 1.3.2 Control Message Types and section 14.3 to account for new feed start time of 2:00am ET<br><br>Removed support for Next Day Settlement from Order Execution Message and Non-Displayed Trade message |
| 2.5d | 11/15/2024 | Replaced Regulatory Imbalance language with Significant Imbalance language throughout document and imbalance message (msg type 105) structure. Removed significant imbalance indicator from imbalance message (msg type 105) and replaced with reserved field |
| 2.5e | 03/28/2025 | Rebranded NYSE Chicago to NYSE Texas |
| 2.5f | 05/17/2025 | Updated Appendix A: Information on Auctions to reflect NYSE Texas auctions |
| 2.5g | 10/23/2025 | Removed Auction information sections and added references to the NYSE Group Pillar Equities Functional Differences document.<br><br>Added information about upcoming change to do imbalance calculations at reference price to Imbalance Message and updated descriptions to refer to NYSE Group Pillar Equities Functional Differences document |

### REFERENCE MATERIAL

The following lists the associated documents, which either should be read in conjunction with this document or which provide other relevant information for the user:

- Common Client Specification
- ICE Global Network
- NYSE Symbology
- IP Addresses

---

## CONTACT INFORMATION

### Service Desk

- **Telephone:** +1 212 896-2830
- **Email:** support@nyse.com

---

## FURTHER INFORMATION

For additional information about the product, visit the [Integrated Feed Product Page](https://www.nyse.com/market-data/integrated-feed).

For updated capacity figures, visit the Market Data [capacity page](https://www.nyse.com/market-data/capacity).

---

## TABLE OF CONTENTS

1. [INTEGRATED FEED](#1-integrated-feed)
   - 1.1 [Overview](#11-overview)
   - 1.2 [Control Message Types Used in the feed](#12-control-message-types-used-in-the-feed)
   - 1.3 [Message Publication Times](#13-message-publication-times)
2. [ADD ORDER MESSAGE – MSG TYPE 100](#2-add-order-message--msg-type-100)
3. [MODIFY ORDER MESSAGE – MSG TYPE 101](#3-modify-order-message--msg-type-101)
4. [DELETE ORDER MESSAGE – MSG TYPE 102](#4-delete-order-message--msg-type-102)
5. [ORDER EXECUTION MESSAGE – MSG TYPE 103](#5-order-execution-message--msg-type-103)
6. [REPLACE ORDER MESSAGE – MSG TYPE 104](#6-replace-order-message--msg-type-104)
7. [IMBALANCE MESSAGE – MSG TYPE 105](#7-imbalance-message--msg-type-105)
8. [ADD ORDER REFRESH MESSAGE – MSG TYPE 106](#8-add-order-refresh-message--msg-type-106)
9. [NON-DISPLAYED TRADE MESSAGE – MSG TYPE 110](#9-non-displayed-trade-message--msg-type-110)
10. [CROSS TRADE MESSAGE – MSG TYPE 111](#10-cross-trade-message--msg-type-111)
11. [TRADE CANCEL MESSAGE – MSG TYPE 112](#11-trade-cancel-message--msg-type-112)
12. [CROSS CORRECTION MESSAGE – MSG TYPE 113](#12-cross-correction-message--msg-type-113)
13. [RETAIL PRICE IMPROVEMENT MESSAGE – MSG TYPE 114](#13-retail-price-improvement-message--msg-type-114)
14. [STOCK SUMMARY MESSAGE – MSG TYPE 223](#14-stock-summary-message--msg-type-223)

[APPENDIX A: PRODUCT IDS](#appendix-a-product-ids)

---

## 1. INTEGRATED FEED

### 1.1 OVERVIEW

The Integrated Feed provides real-time market data in a unified view of events, in sequence, as they appear on the Pillar matching engines. The Integrated Feed includes depth of book order data, last sale data, and opening and closing imbalance data. The Integrated Feed also includes security status updates (e.g., trade corrections and trading halts) and stock summary messages.

All message types defined in this document appear only in the main publication channels, except:

- The Imbalance and Add Order Refresh message types also appear in the Refresh channels.
- The Stock Summary message appears only in a dedicated Stock Summary channel.

See the Common Client Specification for details on Time Reference and Symbol Index Mapping messages, and Order ID and Price field formats.

### 1.2 CONTROL MESSAGE TYPES USED IN THE FEED

See the Common Client Specification for details on all control messages.

| MSG TYPE | DESCRIPTION | PUBLISHER CHANNELS | REQUEST CHANNEL | REFRESH CHANNELS |
|----------|-------------|-------------------|-----------------|------------------|
| 1 | Sequence Number Reset | x | | x |
| 2 | Time Reference | x | | x |
| 3 | Symbol Index Mapping | x | | x |
| 10 | Retransmission Request | | client | |
| 11 | Request Response | | server | |
| 12 | Heartbeat Response | | client | |
| 13 | Symbol Index Mapping Request | | client | |
| 15 | Refresh Request | | client | |
| 31 | Message Unavailable | | server | |
| 32 | Symbol Clear | x | | |
| 34 | Security Status Message | x | | x |
| 35 | Refresh Header Message | | | x |

### 1.3 MESSAGE PUBLICATION TIMES

Scheduled trading session times on normal and early-close days for all NYSE markets can be found [here](https://www.nyse.com/markets/hours-calendars).

#### 1.3.1 Integrated Feed Message Types and Hours

Hours are for guidelines use only (e.g. publication times for NYSE Tape A are not exactly at 4:00pm ET, rather when the stock is closed).

| MSG TYPE | DESCRIPTION | HOURS |
|----------|-------------|-------|
| 100 | Add Order Message | **NYSE Arca:** 2:30am – 8:00pm<br>**NYSE American:** 6:30am – 8:00pm<br>**NYSE Texas:** 6:30am – 8:00pm<br>**NYSE National:** 6:30am – 8:00pm<br>**NYSE Tape A, B & C:** 6:30am – 4:00pm |
| 101 | Modify Order Message | |
| 104 | Replace Order Message | |
| 102 | Delete Order Message | |
| 112 | Trade Cancel Message | |
| 111 | Cross Trade Message | |
| 113 | Cross Correction Message | |
| 114 | Retail Price Improvement Msg | |
| 106 | Add Order Refresh Message | |
| 105 | Imbalance Message | See Appendix A |
| 103 | Order Execution Message | **NYSE Arca:** 4:00am – 8:00pm<br>**NYSE American:** 7:00am – 8:00pm<br>**NYSE National:** 7:00am – 8:00pm<br>**NYSE Tape A:** 9:30am – 4:00pm<br>**NYSE Tapes B&C:** 7:00am – 4:00pm<br>**NYSE Texas:** 7:00am – 8:00pm |
| 110 | Non-Displayed Trade Message | |
| 223 | Stock Summary Message | |

#### 1.3.2 Control Message Types

The initial publication of messages occurs shortly after feed start time. For the exact timing on each market, refer to the Common Client Specification, section "Proprietary Data - Production Hours."

---

## 2. ADD ORDER MESSAGE – MSG TYPE 100

An Add Order message is published when a new visible order has been received and added to the book. The Order ID is assigned by the matching engine and is good for current day only. If an order, which was previously displayed, gets routed to an away market and upon returning unexecuted there is no residual of this order, another AddOrder message will be published with the same order ID.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 39 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>100 – Add Order Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 16 | 8 | Binary | The unique ID assigned by the matching engine to this order. Can be used to match this message to the gateway Order Report. |
| Price | 24 | 4 | Binary | The order price. Use with the Price Scale from the symbol-mapping index. |
| Volume | 28 | 4 | Binary | The order quantity in shares |
| Side | 32 | 1 | ASCII | The side of the order (Buy/Sell). Valid values:<br>• 'B' – Buy<br>• 'S' – Sell |
| FirmID | 33 | 5 | ASCII | The market participant's firm ID.<br>Blank-filled if a firm ID was not specified and/or if the order is not marked as Attributed. |
| Reserved 1 | 38 | 1 | Binary | Defaulted to 0. Future use only. |

---

## 3. MODIFY ORDER MESSAGE – MSG TYPE 101

A Modify Order message is sent when the price or volume of an order is changed due to an event other than a cancel-replace, or full or partial execution. The content of the price and volume fields represent the new values after modification.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 35 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>101 – Modify Order Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 16 | 8 | Binary | The unique ID assigned by the matching engine to the order to be modified |
| Price | 24 | 4 | Binary | The new order price. Use the Price scale from the symbol mapping index. |
| Volume | 28 | 4 | Binary | The new order quantity in shares. |
| PositionChange | 32 | 1 | Binary | Currently defaulted to 0.<br>• '0' – Kept position in book<br>• '1' – Lost position in book<br><br>If the price of the Modify Order didn't change, order always keeps the position in the book.<br>If the price of the Modify Order message changed, the order always loses position. |
| Side | 33 | 1 | ASCII | The side of the order. Valid values:<br>• 'B' – Buy<br>• 'S' – Sell |
| Reserved 2 | 34 | 1 | Binary | Defaulted to 0. Future use only. |

---

## 4. DELETE ORDER MESSAGE – MSG TYPE 102

A Delete Order message is published when an order is taken off of the book for any reason except for full execution, in which case an Order Execution message is sent. If the order is replaced, a delete order message will not be published, rather a Replace Order message.

Immediately before a trading session changes (eg: Early session to Core session), all orders that were submitted for the current or current+previous sessions are explicitly deleted with a Delete Order message.

Please note that when trading in a security is closed for the day, a Security Status 'X' is sent and unexecuted orders are cancelled, but explicit Delete Order Messages are not sent.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 25 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>102 – Delete Order Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 16 | 8 | Binary | The unique ID assigned by the matching engine to the existing order to be deleted |
| Reserved 1 | 24 | 1 | Binary | Defaulted to 0. Future use only. |

---

## 5. ORDER EXECUTION MESSAGE – MSG TYPE 103

An Order Execution message is sent when an order is partially or fully executed. If the Price field is different from the price of the order, any remaining shares keep their original price. If the Volume field equals the number of shares previously remaining in the order, then the order has been fully executed and should be removed from the book. If the order has been partially executed, further Order Execution messages for this Order ID may be published.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 42 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>103 – Order Execution Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 16 | 8 | Binary | The unique ID assigned by the matching engine to the partially or fully executed order. |
| TradeID | 24 | 4 | Binary | Unique ID assigned by the matching engine to this execution. Used by any subsequent Trade Cancel message to identify this execution. Can be matched to the Deal ID field in the gateway Execution Report. |
| Price | 28 | 4 | Binary | The execution price of this trade. Use the Price Scale from the symbol mapping index. |
| Volume | 32 | 4 | Binary | The executed quantity in shares. |
| PrintableFlag | 36 | 1 | Binary | • 0 = Not Printed to the SIP<br>• 1 = Printed to the SIP<br><br>All individual executions (Execution messages and Non-Displayed Trade messages) will have Printable Flag set to 0 for auction trades, so that the auction volume is not double counted. |
| Reserved 1 | 37 | 1 | Binary | Defaulted to 0. Future use only. |
| TradeCond1 | 38 | 1 | ASCII | Settlement related conditions. Valid values:<br>• @ – Regular Sale<br>• 'C' – Cash (Texas only) |
| TradeCond2 | 39 | 1 | ASCII | The reason for Trade Through Exemptions. Valid values:<br>• ' ' – N/A (0x20)<br>• 'F' – Intermarket Sweep Order<br>• 'O' – Market Center Opening Trade (Arca, American and NYSE only)<br>• '5' - Reopening Trade (Arca, American and NYSE only)<br>• '6' – Market Center Closing Trade (Arca, American and NYSE only)<br>• '7' – Qualified Contingent Trade (Texas only) |
| TradeCond3 | 40 | 1 | ASCII | Extended hours/sequencing related conditions. Valid values:<br>• ' ' – (space, or 0x20) N/A<br>• 'T' – Extended Hours Trade<br>• 'U' – Extended Hours Sold (Out of Sequence)<br>• 'Z' – Sold |
| TradeCond4 | 41 | 1 | ASCII | SRO Required Detail. Valid values:<br>• ' ' – (space, or 0x20) N/A<br>• 'I' – Odd Lot Trade<br>• 'V' – Contingent Trade (Texas only) |

---

## 6. REPLACE ORDER MESSAGE – MSG TYPE 104

A Replace Order message is published when a cancel/replace order is received and executed. The sitting order is replaced with a new one containing the same symbol, side and attribution, a new Order ID, and the price and size specified. The sitting order must be removed from the book and replaced with the new order.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 42 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>• 104 – Replace Order Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 16 | 8 | Binary | The unique ID assigned by the matching engine to the existing order to be replaced |
| NewOrderID | 24 | 8 | Binary | The new Order ID of the replacement order |
| Price | 32 | 4 | Binary | The new order price. Use the Price scale from the symbol mapping index. |
| Volume | 36 | 4 | Binary | The new order quantity in shares. |
| Side | 40 | 1 | ASCII | Side of the order. Valid values:<br>• 'B' - Buy<br>• 'S' - Sell |
| Reserved 2 | 41 | 1 | Binary | Defaulted to 0. Future use only. |

---

## 7. IMBALANCE MESSAGE – MSG TYPE 105

Imbalance messages are published once a second during auctions to update price and volume information. If there is no change to the calculated fields, no message will be generated. More Auction Information can be found in the NYSE Group Pillar Equities Functional Differences Document.

| FIELD NAME | OFFSET | SIZE | FORMAT | DESCRIPTION | NYSE | AMERICAN | ARCA | TEXAS |
|------------|--------|------|--------|-------------|------|----------|------|-------|
| Msg Size | 0 | 2 | Binary | Size of the message: 73 bytes | Yes | Yes | Yes | Yes |
| Msg Type | 2 | 2 | Binary | This field identifies the type of message.<br>105 – Imbalance Message | Yes | Yes | Yes | Yes |
| SourceTime | 4 | 4 | Binary | The time when this msg was generated in the order book, in secs since 1/1/1970 00:00:00 UTC | Yes | Yes | Yes | Yes |
| SourceTimeNS | 8 | 4 | Binary | The nanosecond offset from the Source Time | Yes | Yes | Yes | Yes |
| SymbolIndex | 12 | 4 | Binary | The ID of the symbol in the Symbol Index msg | Yes | Yes | Yes | Yes |
| SymbolSeqNum | 16 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol | Yes | Yes | Yes | Yes |
| ReferencePrice | 20 | 4 | Binary | The price at which imbalances are calculated*<br>For more information, see item 48 and 49 in the NYSE Group Pillar Equities Functional Differences Document | Yes | Yes | Yes | Yes |
| PairedQty | 24 | 4 | Binary | Number of shares paired at the Reference Price. For more information, see item 50 in the NYSE Group Pillar Equities Functional Differences Document | Yes | Yes | Yes | Yes |
| TotalImbalanceQty | 28 | 4 | Binary | The total imbalance quantity at the Reference Price*. | Yes | Yes | Yes | Yes |
| MarketImbalanceQty | 32 | 4 | Binary | The total market order imbalance quantity at the Reference Price*. | No | Yes | Yes | Yes |
| AuctionTime | 36 | 2 | Binary | Projected Auction Time (hhmm) | Yes | Yes | Yes | Yes |
| AuctionType | 38 | 1 | ASCII | • 'O' – Early Opening Auction<br>• 'M' – Core Opening Auction<br>• 'H' – Reopening Auction (Halt resume)<br>• 'C' – Closing Auction<br>• 'P' – Extreme Closing Imbalance<br>• 'R' – Significant Closing Imbalance | M<br>H<br>C<br>P<br>R | O<br>M<br>H<br>C | O<br>M<br>H<br>C | O<br>M<br>H<br>C |
| ImbalanceSide | 39 | 1 | ASCII | The side of the TotalImbalanceQty<br>• 'B' – Buy side<br>• 'S' – Sell side<br>• ' ' - (space or 0x20) – No imbalance | B<br>S<br>' ' | B<br>S<br>' ' | B<br>S<br>' ' | B<br>S<br>' ' |
| ContinuousBook ClearingPrice | 40 | 4 | Binary | The price closest to the reference price where the imbalance is 0. For more information, see item 51 in the NYSE Group Pillar Equities Functional Differences Document | Yes | Yes | Yes | Yes |
| AuctionInterest ClearingPrice | 44 | 4 | Binary | The price at which auction only interest would trade. For more information, see item 52 in the NYSE Group Pillar Equities Functional Differences Document | Yes | Yes | Yes | Yes |
| SSRFilingPrice | 48 | 4 | Binary | For NYSE non-Significant imbalances, if a Sell Short Restriction is in effect, the price at which Sell Short interest will be filed. | Yes | No | No | No |
| IndicativeMatchPrice | 52 | 4 | Binary | The best price at which the maximum volume of shares is executable in the applicable auction, subject to Auction Collars. For more information, see item 48 in the NYSE Group Pillar Equities Functional Differences Document | No | Yes | Yes | Yes |
| UpperCollar | 56 | 4 | Binary | Upper boundary for the Indicative Match Price. For more information, see item 10 in the NYSE Group Pillar Equities Functional Differences Document | No | Yes | Yes | Yes |
| LowerCollar | 60 | 4 | Binary | Lower boundary for the Indicative Match Price. For more information, see item 10 in the NYSE Group Pillar Equities Functional Differences Document | No | Yes | Yes | Yes |
| AuctionStatus | 64 | 1 | Binary | Indicates whether the auction will run<br>• 0 - Will run as always for Open and Close<br>• 1 - Will run, interest exists inside or at the collars or is fully paired off<br>• 2 – Will not run because there is an imbalance through the collars<br>• 3 – Will not run, will transition to the Closing Auction instead | 0<br>3 | 0<br>1<br>2<br>3 | 0<br>1<br>2<br>3 | 0<br>1<br>2<br>3 |
| FreezeStatus | 65 | 1 | Binary | Indicates an Imbalance Freeze for the auction. For more information, see item 1 in the NYSE Group Pillar Equities Functional Differences Document.<br>• 0 - Imbalance freeze not in effect<br>• 1 - Imbalance freeze is in effect | 0<br>1 | 0<br>1 | 0<br>1 | 0<br>1 |
| NumExtensions | 66 | 1 | Binary | The number of times a halt period has been extended | No | Yes | Yes | Yes |
| Unpaired Qty | 67 | 4 | Binary | The number of unpaired shares priced at or better than the Reference Price. | Yes | No | No | No |
| Unpaired Side | 71 | 1 | ASCII | The side of the Unpaired Qty<br>• 'B' - buy side<br>• 'S' - sell side<br>• ' ' - (space or 0x20) - not applicable | B<br>S<br>' ' | ' ' | ' ' | ' ' |
| Reserved | 72 | 1 | ASCII | Reserved for future use | | | | |

**Note:** *calculations at indicative match price will move to calculations at reference price on 11/10/25 for American, 11/13/25 for Arca, 11/20/25 for Texas

---

## 8. ADD ORDER REFRESH MESSAGE – MSG TYPE 106

The Add Order Refresh message can be sent in either of two contexts:

1. If a client sends a Refresh Request to the Pillar Request Server, an Add Order Refresh message is sent over the Refresh channels as part of the refresh response for every order currently sitting on the book.
2. If NYSE Operations refreshes a symbol, a Symbol Clear message is published, followed by a full refresh. The refresh includes an Add Order Refresh message for every order currently sitting on the book of the symbol.

See the Common Client Specification for details on Time Reference and Symbol Index Mapping messages, and Order ID and Price field formats.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 43 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>106 – Add Order Refresh Message |
| SourceTime | 4 | 4 | Binary | The time when this msg was generated in the order book, in seconds since Jan 1, 1970 00:00:00 UTC. |
| SourceTimeNS | 8 | 4 | Binary | The nanosecond offset from the SourceTime |
| SymbolIndex | 12 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 16 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| OrderID | 20 | 8 | Binary | The unique ID assigned by the matching engine to this order |
| Price | 28 | 4 | Binary | The order price. Use the Price scale from the symbol-mapping index. |
| Volume | 32 | 4 | Binary | The order quantity in shares. |
| Side | 36 | 1 | ASCII | The side of the order (Buy/sell). Valid values:<br>• 'B' – Buy<br>• 'S' – Sell |
| FirmID | 37 | 5 | ASCII | The participant's firm ID, or blanks if firm ID was not specified OR if the orer is not marked as attributed. |
| Reserved 1 | 42 | 1 | Binary | Defaulted to 0. Future use only. |

---

## 9. NON-DISPLAYED TRADE MESSAGE – MSG TYPE 110

An Non Displayed Trade message is sent as a result of a match between two non-displayed orders.

Customers who are only interested in building a book of displayed orders may safely ignore Non-Displayed Trade messages. Customers who are creating statistics or displays requiring the full record of trades in this market will need to process Non-Displayed Trade messages.

If a MOC order executes against MOC or LOC, then a Non-Displayed Trade message is published. If a MOC order was executed against a displayed order, only an 'Order Execution' message for the displayed order will be published.

See the Common Client Specification for details on Time Reference and Symbol Index Mapping messages, and Order ID, Trade ID, and Price field formats.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 33 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>110 – Non-Displayed Trade Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| TradeID | 16 | 4 | Binary | Unique ID assigned by the matching engine to this trade event. Used by any subsequent Trade Cancel message to identify this execution. Can be matched to the Deal ID field in the gateway Execution Report. |
| Price | 20 | 4 | Binary | The execution price of the trade. Use the Price scale from the symbol mapping index. |
| Volume | 24 | 4 | Binary | Volume of the trade in shares |
| PrintableFlag | 28 | 1 | Binary | • 0 = Not Printed to the SIP<br>• 1 = Printed to the SIP |
| TradeCond1 | 29 | 1 | ASCII | Settlement related conditions. Valid values:<br>• '@' – Regular Sale<br>• 'C' – Cash (Texas only) |
| TradeCond2 | 30 | 1 | ASCII | The reason for Trade Through Exemptions. Valid values:<br>• ' ' – (space or 0x20)<br>• 'F' – Intermarket Sweep Order<br>• 'O' – Market Center Opening Trade (Arca, American and NYSE only)<br>• '5' - Reopening Trade (Arca, American and NYSE only)<br>• '6' – Market Center Closing Trade (Arca, American and NYSE only)<br>• '7' – Qualified Contingent Trade (Texas only) |
| TradeCond3 | 31 | 1 | ASCII | Extended hours/sequencing related conditions. Valid values:<br>• ' ' – (space or 0x20)<br>• 'T' – Extended Hours Trade<br>• 'U' – Extended Hours Sold (Out of Sequence)<br>• 'Z' – Sold |
| TradeCond4 | 32 | 1 | ASCII | SRO Required Detail. Valid values:<br>• ' ' – (space or 0x20)<br>• 'I' – Odd Lot Trade<br>• 'V' – Contingent Trade (Texas only) |

---

## 10. CROSS TRADE MESSAGE – MSG TYPE 111

A Cross Trade message is published on completion of a crossing auction, and shows the bulk volume that traded in the auction. The Reason Code field indicates the auction type. Additionally, a non-printable Order Execution or Non-Displayed Trade message will be published for each order that traded.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 29 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>111 – Cross Trade Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| CrossID | 16 | 4 | Binary | Unique identifier for this Cross Trade. Used in Cross Correction message to identify the Cross Trade to correct. |
| Price | 20 | 4 | Binary | The execution price. Use the Price scale from the symbol mapping index. |
| Volume | 24 | 4 | Binary | Volume executed in shares |
| CrossType | 28 | 1 | ASCII | Reason for the crossing auction. Valid values:<br>• 'E' – Market Center Early Opening Auction<br>• 'O' – Market Center Opening Auction<br>• '5' – Market Center Reopening Auction<br>• '6' – Market Center Closing Auction |

---

## 11. TRADE CANCEL MESSAGE – MSG TYPE 112

In the event that an earlier trade has been reported in error, a Trade Cancel message is sent. This occurs whether the initial report was an Order Execution or a Non-Displayed Trade message.

Note that since Trade Cancel messages only affect trades that occurred in the past, customers who are only interested in building a book may safely ignore them.

Customers who are building a complete record of today's trades should remove the cancelled trade from their records and subtract its volume from any statistics.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 20 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>• 112 – Trade Cancel Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| TradeID | 16 | 4 | Binary | The TradeID of the original Trade or Execution message to be cancelled. |

---

## 12. CROSS CORRECTION MESSAGE – MSG TYPE 113

In the event that an earlier Cross Trade has been reported in error, a Cross Correction message is sent.

Note that since Cross Correction messages only affect cross auctions that occurred in the past, customers who are only interested in building a book may safely ignore them.

Customers who are building a complete record of current day volume should remove the previously reported volume from their statistics and add the volume of the Cross Correction to them.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 24 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>113 – Cross Correction Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| CrossID | 16 | 4 | Binary | The CrossID of the original Cross Trade message to be corrected. |
| Volume | 20 | 4 | Binary | The corrected volume of Cross Trade message. |

---

## 13. RETAIL PRICE IMPROVEMENT MESSAGE – MSG TYPE 114

Published when RPI interest (hidden retail price improvement interest) is added or removed between the best bid and best offer price. When all RPI interest for this security is removed from the book, an RPI message with RPIIndicator = ' ' (space character) is published.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 17 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>• 114 – Retail Price Improvement Message |
| SourceTimeNS | 4 | 4 | Binary | The nanosecond offset from the Time Reference second (since Jan 1, 1970 00:00:00 UTC) |
| SymbolIndex | 8 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| SymbolSeqNum | 12 | 4 | Binary | The sequence number of this message in the set of all messages for this symbol |
| RPIIndicator | 16 | 1 | ASCII | The side(s) where Retail Price Improvement orders (RPI orders) exist. Valid values correspond to CQS values:<br>• ' ' – (space or 0x20) means no retail interest (default)<br>• 'A' – Retail interest on the bid side<br>• 'B' – Retail interest on the offer side<br>• 'C' – Retail interest on the bid and offer sides. |

---

## 14. STOCK SUMMARY MESSAGE – MSG TYPE 223

A Stock Summary message per symbol is sent every 60 seconds, on a separate Stock Summary channel from the main feed.

The message is sent regardless of whether there has been a change to the symbol in the last 60 seconds or not.

See the Common Client Specification for details on the Price field format.

| FIELD NAME | OFFSET | SIZE (BYTES) | FORMAT | DESCRIPTION |
|------------|--------|--------------|--------|-------------|
| Msg Size | 0 | 2 | Binary | Size of the message: 36 bytes |
| Msg Type | 2 | 2 | Binary | The type of message:<br>• 223 – Stock Summary Message |
| SourceTime | 4 | 4 | Binary | The time when this msg was generated in the order book, in seconds since Jan 1, 1970 00:00:00 UTC. |
| SourceTimeNS | 8 | 4 | Binary | The nanosecond offset from the SourceTime |
| SymbolIndex | 12 | 4 | Binary | The ID of the symbol in the Symbol Index msg |
| HighPrice | 16 | 4 | Binary | The exchange high price of this stock for the day. Use the Price Scale in the symbol index msg. |
| LowPrice | 20 | 4 | Binary | The exchange Low price of this stock for the day. Use the Price Scale in the symbol index msg. |
| Open | 24 | 4 | Binary | The exchange Opening price of this stock for the day. Use the Price Scale in the symbol index msg. |
| Close | 28 | 4 | Binary | The exchange Closing price of this stock for the day. Use the Price Scale in the symbol index msg. |
| TotalVolume | 32 | 4 | Binary | The exchange cumulative volume for the stock throughout the day. |

---

## APPENDIX A: PRODUCT IDS

Refresh and Retransmission Request messages must specify a Product ID. The correct product IDs for the Integrated Feeds:

| EXCHANGE | PRODUCT ID | DESCRIPTION |
|----------|-----------|-------------|
| NYSE | 11 | NYSE Integrated Feed |
| NYSE American | 59 | NYSE American Integrated Feed |
| NYSE National | 109 | NYSE National Integrated Feed |
| NYSE Arca | 157 | NYSE Arca Integrated Feed |
| NYSE Texas | 209 | NYSE Texas Integrated Feed |

---

**END OF DOCUMENT**
