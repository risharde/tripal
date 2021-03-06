<?php
/**
 * @file
 * Provides API functions specifically for managing analysis records in Chado.
 *
 * @ingroup tripal_chado
 */

/**
 * @defgroup tripal_analysis_api Chado Analysis
 * @ingroup tripal_chado_api
 * @{
 * Provides API functions for working with analysis records in Chado that
 * go beyond the generic Chado API functions.
 * @}
 */


/**
 * Retrieves a chado analysis variable.
 *
 * @param $identifier
 *   an array with the key stating what the identifier is. Supported keys
 *   (only on of the following unique keys is required):
 *    - analysis_id: the chado analysis.analysis_id primary key.
 *    - nid: the drupal node.nid primary key.
 *   There are also some specially handled keys. They are:
 *    - property: An array/object describing the property to select records for.
 *      It should at least have either a type_name (if unique across cvs) or
 *      type_id. Other supported keys include: cv_id/cv_name (of the type),
 *      value and rank.
 *   NOTE: the $identifier parameter can be any array similar to $values
 *   passed into chado_select_record(). It should fully specify the stock record
 *   to be returned.
 * @param $options
 *   An array of options. Supported keys include:
 *     - Any keys supported by chado_generate_var(). See that function
 *       definition for additional details.
 *
 * @return
 *   the analysis node matching the passed in identifier
 *
 * @ingroup tripal_analysis_api
 */
function chado_get_analysis($identifier, $options) {

  // Set Defaults
  if (!isset($options['include_fk'])) {
    // Tells chado_generate_var not to follow any foreign keys.
    $options['include_fk'] = [];
  }

  // Error Checking of parameters.
  if (!is_array($identifiers)) {
    tripal_report_error(
      'tripal_stock_api',
      TRIPAL_ERROR,
      "chado_get_stock: The identifier passed in is expected to be an array with the key
        matching a column name in the analysis table (ie: analysis_id or name). You passed in %identifier.",
      [
        '%identifier' => print_r($identifiers, TRUE),
      ]
    );
  }
  elseif (empty($identifiers)) {
    tripal_report_error(
      'tripal_stock_api',
      TRIPAL_ERROR,
      "chado_get_stock: You did not pass in anything to identify the analysis you want. The identifier
        is expected to be an array with the key matching a column name in the analysis table
        (ie: stock_id or name). You passed in %identifier.",
      [
        '%identifier' => print_r($identifiers, TRUE),
      ]
    );
  }

  // If one of the identifiers is property then use chado_get_record_with_property().
  if (isset($identifiers['property'])) {
    $property = $identifiers['property'];
    unset($identifiers['property']);
    $analysis = chado_get_record_with_property(
      ['table' => 'analysis', 'base_records' => $identifiers],
      ['type_name' => $property]
    );
  }

  // Else we have a simple case and we can just use chado_generate_var to get 
  // the analysis.
  else {

    // Try to get the analysis
    $analysis = chado_generate_var(
      'analysis',
      $identifiers,
      $options
    );
  }

  // Ensure the analysis is singular. If it's an array then it is not singular.
  if (is_array($analysis)) {
    tripal_report_error(
      'tripal_analysis_api',
      TRIPAL_ERROR,
      "chado_get_analysis: The identifiers you passed in were not unique. You passed in %identifier.",
      [
        '%identifier' => print_r($identifiers, TRUE),
      ]
    );
  }

  // Report an error if $analysis is FALSE since then chado_generate_var has 
  // failed.
  elseif ($analysis === FALSE) {
    tripal_report_error(
      'tripal_analysis_api',
      TRIPAL_ERROR,
      "chado_get_analysis: chado_generate_var() failed to return a analysis based on the identifiers
        you passed in. You should check that your identifiers are correct, as well as, look
        for a chado_generate_var error for additional clues. You passed in %identifier.",
      [
        '%identifier' => print_r($identifiers, TRUE),
      ]
    );
  }

  // Else, as far we know, everything is fine so give them their analysis :)
  else {
    return $analysis;
  }
}

/**
 * Returns a list of analyses that are currently synced with Drupal to use in
 * select lists.
 *
 * @param $syncd_only
 *   Whether or not to return all chado analyses or just those sync'd with
 *   drupal. Defaults to TRUE (only sync'd analyses).
 *
 * @return
 *   An array of analyses sync'd with Drupal where each value is the analysis
 *   scientific name and the keys are analysis_id's.
 *
 * @ingroup tripal_analysis_api
 */
function chado_get_analysis_select_options($syncd_only = TRUE) {
  $analysis_list = [];
  $analysis_list[] = 'Select an analysis';

  if ($syncd_only) {
    $sql = "
      SELECT *
      FROM [chado_analysis] CA
        INNER JOIN {analysis} A ON A.analysis_id = CO.analysis_id
      ORDER BY A.name
    ";
    $analyses = chado_query($sql);

    // iterate through the analyses and build an array of those that are synced.
    foreach ($analyses as $analysis) {
      $analysis_list[$analysis->analysis_id] = $analysis->name;
    }
  }
  else {
    // use this SQL statement for getting the analyses
    $csql = "SELECT * FROM {analysis} ORDER BY name";
    $analyses = chado_query($csql);

    // iterate through the analyses and build an array of those that are synced.
    foreach ($analyses as $analysis) {
      $analysis_list[$analysis->analysis_id] = $analysis->name;
    }
  }
  return $analysis_list;
}
